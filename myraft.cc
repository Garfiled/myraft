#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <unistd.h>
#include <map>
#include <grpcpp/grpcpp.h>

#include "timer.h"
#include "raft.h"
#include "logutils.h"


int startHttpWorker(int,int,RaftCore*);

int RaftCore::propose(Entry* e,void (*cb)(void*,int),void* cb_arg) {
	if (e==nullptr)
		return -1;
	this->mu.lock();
	if (this->leader == 0) {
		this->mu.unlock();
		return -1;
	}
	if (this->state != Leader) {
		int l = this->leader;
		this->mu.unlock();
		return l;
	}
	
	RaftMsg* msg = this->makePropMsg(e);
	msg-> callback = cb;
	msg-> callback_arg = cb_arg;

	this->msg_wal_vec.push_back(msg);
	this->mu.unlock();
	this->msg_wal_cv.notify_one();
	return 0;
}

RaftCore::RaftCore(int _id) {
	this->id = _id;
	this->leader = 0;
	this->state = Follower;
	this->term = 0;
	this->voteFor = 0;
	this->lastLogTerm = 0;
	this->lastLogIndex = 0;
}

void tick(void* arg) {
	RaftCore* rc =(RaftCore*)arg;
	rc->mu.lock();
	if (rc->state == Leader) {
		LOGD("node.%lld leader tick",rc->id);
		RaftMsg* m = new RaftMsg(msg_hub);
		m->term = rc->term;
		m->id = rc->id;
		m->log_term = rc->lastLogTerm;
		m->log_index = rc->lastLogIndex;
		rc->msg_wal_vec.push_back(m);
		rc->mu.unlock();
		rc->msg_wal_cv.notify_one();
	} else {
		rc->mu.unlock();
		rc->hub_ticker->stop();
	}	
}

void startElection(void* arg) {
	LOGI("startElection");
	RaftCore* rc =(RaftCore*)arg;
	RaftMsg* msg = new RaftMsg(msg_vote);
	rc->mu.lock();
	rc->state = Candidate;
	rc->leader = 0;
	rc->term++;
	rc->voteFor = rc->id;


	msg->id = rc->id;
	msg->term = rc->term;
	msg->log_term = rc->lastLogTerm;
	msg->log_index = rc->lastLogIndex;

	rc->msg_wal_vec.push_back(msg);
	rc->mu.unlock();
	rc->msg_wal_cv.notify_one();
}


RaftMsg* RaftCore::makePropMsg(Entry* e)
{
	RaftMsg* m = new RaftMsg(msg_prop);
	
	m->id = this->id;
	m->term = this->term;
	m->log_term = this->lastLogTerm;
	m->log_index = this->lastLogIndex;

	e->term = this->term;
	e->index = this->lastLogIndex+1; 

	m->ents.push_back(e);

	this->ents.push_back(e);

	this->lastLogTerm = e->term;
	this->lastLogIndex = e->index;
	
	return m;
}

void RaftCore::resetElectionTimer()
{
	this->tm.reset_timer(this->election_timer,rand()%1000+1000);

}

void RaftCore::resetHeartBeatTick()
{
	this->tm.reset_timer(this->hub_ticker,400);
}

int RaftCore::vote_back(int happenTerm,bool win,int peerTerm) {
	int becomeLeader = 0;
	this->mu.lock();
	if (this->state == Candidate && this->term == happenTerm && win) {
		this->state = Leader;
		this->leader = id;

		// 生成一条noop entry 防止幽灵日志事件
		auto e = new Entry();
		RaftMsg* msg = this->makePropMsg(e);
		this->msg_wal_vec.push_back(msg);
		this->mu.unlock();
		this->msg_wal_cv.notify_one();
		this->resetHeartBeatTick();
		becomeLeader = 1;
	} else {
		this->state = Follower;
		this->leader = 0;
		this->voteFor = 0;
		if (this->term < peerTerm)
			this->term = peerTerm;
		this->mu.unlock();
		this->resetElectionTimer();
	}
	return becomeLeader;
}

int RaftNode::handleVote(RaftCore* rc,RaftMsg* msg) 
{
	// requestVote
	int voteForCnt = 1;
	int maxTerm = msg->term;
	for (auto &cc : this->peers)
	{
		if (cc.first == msg->id)
			continue;
		grpc::ClientContext ctx;
		raftpb::ReqVote reqVote;
		reqVote.set_candidateid(msg->id);
		reqVote.set_term(msg->term);
		reqVote.set_lastlogterm(msg->log_term);
		reqVote.set_lastlogindex(msg->log_index);
		raftpb::RespVote respVote;
		grpc::Status s = cc.second->stub_->RequestVote(&ctx,reqVote,&respVote);
		if (s.ok()) {
			if (respVote.term() > maxTerm) {
				maxTerm = respVote.term();
			} 
			if (respVote.votegranted()) {
				voteForCnt++;
			}
		} else {
			LOGI("requestVote: %d %s",s.error_code(), s.error_message().c_str());
		}
	}

	if (voteForCnt>this->peers.size()/2) {
		if (rc->vote_back(msg->term,true,maxTerm)>0) {
			LOGI("node.%lld become leader on term %lld",rc->id,msg->term);
		}
	} else {
		LOGI("node.%lld vote failed on term %lld get %d",rc->id,msg->term,voteForCnt);
		rc->vote_back(msg->term,false,maxTerm);
	}

	return 0;
}

int RaftNode::handleProp(RaftCore* rc,RaftMsg* msg) 
{
	// appendEntries
	int finishAE = 1;
	int maxTerm = msg->term;
	for (auto &cc : this->peers)
	{
		if (cc.first == msg->id)
			continue;
		grpc::ClientContext ctx;
		raftpb::ReqAppendEntry reqAE;
		reqAE.set_term(msg->term);
		reqAE.set_leaderid(msg->id);
		reqAE.set_prevlogterm(msg->log_term);
		reqAE.set_prevlogindex(msg->log_index);

		for (auto e : msg->ents)
		{
			auto pbEntry = reqAE.add_entries();
			pbEntry->set_term(e->term);
			pbEntry->set_index(e->index);
			pbEntry->set_record(e->record);
		}

		raftpb::RespAppendEntry respAE;
		LOGD("AppendEntries call: %lu",msg->ents.size());
		grpc::Status s = cc.second->stub_->AppendEntries(&ctx,reqAE,&respAE);
		LOGD("AppendEntries call end: %lu",msg->ents.size());
		if (s.ok()) {
			if (respAE.success()) {
				finishAE++;
				cc.second->next_index = msg->log_index+msg->ents.size()+1;
			} else {
				if (respAE.term()>maxTerm)
					maxTerm = respAE.term();
			}
		} else {
			LOGI("appendEntry: %d %s",s.error_code(),s.error_message().c_str());
		}
	}

	int err = 0;
	if (finishAE>this->peers.size()/2) {
		err = 0;
	} else {
		err = 1;
	}

	if (msg->callback) {
		msg->callback(msg->callback_arg,err);
	}

	if (maxTerm>msg->term) {
		rc->vote_back(msg->term,false,maxTerm);
	}

	rc->mu.lock();
	LOGD("rc: node.%lld term %lld entry %lu first %d last %d",rc->id,rc->term,rc->ents.size(),rc->ents.front()->index,rc->ents.back()->index);
	
	rc->mu.unlock();
	
	return 0;
}

int RaftNode::handleHub(RaftCore* rc,RaftMsg* msg)
{
	for (auto &cc : this->peers)
	{
		if (cc.first == msg->id)
			continue;

		uint64_t prevLogTerm = 0;
		uint64_t prevLogIndex = 0;
		std::vector<Entry*> todo_ents;
		if (msg->log_index == 0||cc.second->next_index==0||cc.second->next_index>msg->log_index) {
			prevLogTerm = msg->log_term;
			prevLogIndex = msg->log_index;
			todo_ents = msg->ents;
		} else {
			rc->mu.lock();
			if (rc->ents.size()==0) {
				prevLogTerm = msg->log_term;
				prevLogIndex = msg->log_index;
			} else if (cc.second->next_index>rc->ents.back()->index) {
				prevLogTerm = msg->log_term;
				prevLogIndex = msg->log_index;
			} else if (cc.second->next_index>rc->ents.front()->index) {
				prevLogTerm = rc->ents[cc.second->next_index-1-rc->ents.front()->index]->term;
				prevLogIndex = rc->ents[cc.second->next_index-1-rc->ents.front()->index]->index;

				for (int i=cc.second->next_index-rc->ents.front()->index;i<=msg->log_index-rc->ents.front()->index;i++)
				{
					todo_ents.push_back(rc->ents[i]);
				}
			} else {
				if (cc.second->next_index == rc->ents.front()->index) {
					prevLogTerm = 0;
					prevLogIndex = 0;
					todo_ents = rc->ents;
				} else {
					LOGI("entry index not match: need %lld but begin with %d",cc.second->next_index-1,rc->ents.front()->index)
					prevLogTerm = rc->ents.front()->term;
					prevLogIndex = rc->ents.front()->index;
				}
			}
			rc->mu.unlock();
		}

		grpc::ClientContext ctx;
		raftpb::ReqAppendEntry reqAE;
		reqAE.set_term(msg->term);
		reqAE.set_leaderid(msg->id);
		reqAE.set_prevlogterm(prevLogTerm);
		reqAE.set_prevlogindex(prevLogIndex);

		for (auto e : todo_ents)
		{
			auto pbEntry = reqAE.add_entries();
			pbEntry->set_term(e->term);
			pbEntry->set_index(e->index);
			pbEntry->set_record(e->record);
		}

		raftpb::RespAppendEntry respAE;
		grpc::Status s = cc.second->stub_->AppendEntries(&ctx,reqAE,&respAE);
		if (s.ok()) {
			if (respAE.success()) {
				cc.second->next_index = prevLogIndex + todo_ents.size() + 1;
			} else {
				if (respAE.term()>msg->term) {
					rc->vote_back(msg->term,false,respAE.term());
				} else {
					cc.second->next_index = prevLogIndex;
				}
			}
		} else {
			LOGI("hub: %d %s",s.error_code(),s.error_message().c_str());
		}
	}
	return 0;
}

void startRaftNode(RaftNode* rn,RaftCore* rc) 
{
	LOGI("startRaftNode node.%lld", rc->id);
	while (true) 
	{
		std::vector<RaftMsg*> todo;
		{
			std::unique_lock<std::mutex> lk(rn->msg_node_mu);
			if (rn->msg_node_vec.size()==0) {
				rn->msg_node_cv.wait(lk);
				continue;
			}
			todo = rn->msg_node_vec;
			rn->msg_node_vec.clear();		
		}

		for (auto msg : todo)
		{
			if (msg->msg_type == msg_prop) {
				LOGD("node.%lld on term.%lld recv %lu",rc->id,rc->term,msg->ents.size());

				rn->handleProp(rc,msg);

			} else if (msg->msg_type == msg_vote) {
				LOGI("node.%lld start election on term %lld", msg->id,msg->term);
				rn->handleVote(rc,msg);
			} else if (msg->msg_type == msg_hub) {
				rn->handleHub(rc,msg);
			} else {
				LOGI("unsupport msgType:%d", msg->msg_type);
			}

			delete msg;
		}
	}

}

void sysmon(RaftCore* rc) 
{
	rc->tm.detect_timers();
}

void startWalWorker(RaftNode* rn,RaftCore* rc)
{
	while (true) 
	{
		std::vector<RaftMsg*> todo;
		{
			std::unique_lock<std::mutex> lk(rc->mu);
			if (rc->msg_wal_vec.size()==0) {
				rc->msg_wal_cv.wait(lk);
				continue;
			}	
			todo = rc->msg_wal_vec;
			rc->msg_wal_vec.clear();
		}
		
		bool walEvent = false;
		for (auto msg : todo)
		{
			if (msg->msg_type == msg_vote || msg->msg_type == msg_prop) {
				std::string rec;
				msg->encoder(rec);
				rn->wal.writeRecord(rec);

				walEvent = true;
			}
		}
		if (walEvent)
			int syncState = fsync(rn->wal.fd_);

		std::vector<RaftMsg*> todo2;
		for (auto msg : todo)
		{
			if (msg->id == rc->id) {
				todo2.push_back(msg);
			} else {
				if (msg->back)
					msg->back->cv.notify_one();

				delete msg;
			}
		}

		if (todo2.size()>0) {
			rn->msg_node_mu.lock();
			rn->msg_node_vec.insert(rn->msg_node_vec.end(), todo2.begin(), todo2.end());
			rn->msg_node_mu.unlock();
			rn->msg_node_cv.notify_one();
		}
	}

}


// g++ myraft.cc  wal.cc timer.cc raftserviceimpl.cc proto/raftpb.grpc.pb.cc proto/raftpb.pb.cc -std=c++11 -o myraft -lprotobuf -lgrpc++

int main(int argc, char const *argv[])
{
	if (argc<3) {
		LOGI("need param %d",argc);
		return 1;
	}

	int id = atoi(argv[1]);
	if (id<=0) {
		LOGI("id err %s",argv[1]);
		return 1;
	}
	std::vector<std::string> peerAddrs;
	for (int i=2;i<argc;i++) {
		peerAddrs.push_back(std::string(argv[i]));
	}


	srand((unsigned)time(NULL)); 

	RaftNode* rn = new RaftNode();

	// 初始化状态
	std::vector<Entry*> es;
	HardState hs = {0,0};
	
	LOGI("open wal...");
	std::string filename = std::string("./wal.") + std::string(argv[1]);
	int ret = rn->wal.openWal(filename.c_str(),es,hs);
	if (ret!=0) {
		std::cout << "open wal failed " << ret << std::endl;
		return 1;
	}

	LOGI("wal state: term %lld voteFor %lld",hs.term,hs.voteFor);
	int firstIndex = 0;
	int lastIndex = 0;
	if (es.size()>0) {
		firstIndex = es.front()->index;
		lastIndex = es.back()->index;
	}
	LOGI("wal entry: [ %d ... %d ]",firstIndex,lastIndex);

	int startId = 1;
	for (auto addr : peerAddrs)
	{
		RaftClient* client;
		if (startId == id) {
			client = new RaftClient();
		} else {
			client = new RaftClient(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials()));

		}
		client->next_index = 0;
		rn->peers[startId] = client;
		startId++;
	}


	
	RaftCore* rc = new RaftCore(id);
	rc->term = hs.term;
	rc->voteFor = hs.voteFor;

	if (es.size()>0) {
		rc->lastLogTerm = es.back()->term;
		rc->lastLogIndex = es.back()->index;
		rc->ents = es;
	}

	rc->election_timer = new Timer(rc->tm);
	rc->election_timer->create(startElection,1000, Timer::TimerType::ONCE,rc);
	
	rc->hub_ticker = new Timer(rc->tm);
	rc->hub_ticker->create(tick,400, Timer::TimerType::CIRCLE,rc);


	std::thread sysmon_worker(sysmon,rc);
	std::thread wal_worker(startWalWorker,rn,rc);
	std::thread node_worker(startRaftNode,rn,rc);
	std::thread node_service(startRaftService,peerAddrs[id-1],rn,rc);

	rc->resetElectionTimer();

	ret = startHttpWorker(8379,1,rc);
	if (ret!=0) {
		return 1;
	}
	node_worker.join();
	node_service.join();
	wal_worker.join();
	sysmon_worker.join();

	return 0;
}











