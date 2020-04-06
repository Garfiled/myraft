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

int RaftCore::propose(Entry* e) {
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
	auto mb = new MsgBack();
	msg->back = mb;
	this->msg_wal_vec.push_back(msg);
	int size = this->msg_wal_vec.size();
	this->mu.unlock();
	this->msg_wal_cv.notify_one();

	std::unique_lock<std::mutex> lk(mb->mu);
	mb->cv.wait(lk);
	int err = mb->err;
	delete mb;
	return err;
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
		RaftMsg* m = new RaftMsg(msg_hub);
		m->term = rc->term;
		rc->msg_wal_vec.push_back(m);
	}
	int size = rc->msg_wal_vec.size();
	rc->mu.unlock();
	rc->msg_wal_cv.notify_one();
}

void startElection(void* arg) {
	LOGD("startElection");
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
	int size = rc->msg_wal_vec.size();
	rc->mu.unlock();
	rc->msg_wal_cv.notify_one();
}


RaftMsg* RaftCore::makePropMsg(Entry* e)
{
	e->term = this->term;
	e->index = this->lastLogIndex+1; 

	// TODO batch
	RaftMsg* m = new RaftMsg(msg_prop);
	m->id = this->id;
	m->term = this->term;
	m->log_term = this->lastLogTerm;
	m->log_index = this->lastLogIndex;
	m->ents.push_back(e);

	this->lastLogTerm = e->term;
	this->lastLogIndex = e->index;
	return m;
}

void RaftCore::resetElectionTimer()
{
	this->tm.reset_timer(this->election_timer,rand()%1000+5000);

}

int RaftCore::vote_back(int happenTerm,bool win,int peerTerm) {
	int becomeLeader = 0;
	this->mu.lock();
	if (this->state == Candidate && this->term == happenTerm && win) {
		this->state = Leader;
		this->leader = id;

		// 生成一条noop entry 防止幽灵事件
		auto e = new Entry();
		RaftMsg* msg = this->makePropMsg(e);
		this->msg_wal_vec.push_back(msg);
		int size = this->msg_wal_vec.size();
		this->mu.unlock();
		this->msg_wal_cv.notify_one();
		becomeLeader = 1;
	} else {
		this->state = Follower;
		this->leader = 0;
		this->voteFor = 0;
		if (this->term < peerTerm)
			this->term = peerTerm;
		this->mu.unlock();
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
			LOGD("requestVote: %d %s",s.error_code(), s.error_message().c_str());
		}
	}

	if (voteForCnt>this->peers.size()/2) {
		if (rc->vote_back(msg->term,true,maxTerm)>0) {
			LOGD("node.%lld become leader on term %lld",rc->id,msg->term);
		} else {
			rc->resetElectionTimer();	
		}
	} else {
		LOGD("node.%lld vote failed on term %lld get %d",rc->id,msg->term,voteForCnt);
		rc->vote_back(msg->term,false,maxTerm);
		rc->resetElectionTimer();
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
		grpc::Status s = cc.second->stub_->AppendEntries(&ctx,reqAE,&respAE);
		if (s.ok()) {
			if (respAE.success()) {
				finishAE++;
				cc.second->next_index = msg->log_index+msg->ents.size()+1;
			} else {
				if (respAE.term()>maxTerm)
					maxTerm = respAE.term();
			}
		} else {
			LOGD("appendEntry: %d %s",s.error_code(),s.error_message().c_str());
		}
	}

	int err = 0;
	if (finishAE>this->peers.size()/2) {
		err = 0;
	} else {
		err = 1;
	}

	if (msg->back) {
		msg->back->err = err;
		msg->back->cv.notify_one();
	}

	if (maxTerm>msg->term) {
		rc->vote_back(msg->term,false,maxTerm);
	}

	return 0;
}

void startRaftNode(RaftNode* rn,RaftCore* rc) 
{
	LOGD("startRaftNode node.%lld", rc->id);
	while (true) 
	{
		std::vector<RaftMsg*> todo;
		{
			std::unique_lock<std::mutex> lk(rn->msg_node_mu);
			if (rn->msg_node_vec.size()==0) {
				rn->msg_node_cv.wait(lk);
				continue;
			} else {
				todo = rn->msg_node_vec;
				rn->msg_node_vec.clear();
			}
		}

		for (auto msg : todo)
		{
			if (msg->msg_type == msg_prop) {
				LOGD("node.%lld on term.%lld recv %lu",rc->id,rc->term,msg->ents.size());

				rn->handleProp(rc,msg);

			} else if (msg->msg_type == msg_vote) {
				LOGD("node.%lld start election on term %lld", msg->id,msg->term);
				rn->handleVote(rc,msg);
			} else if (msg->msg_type == msg_hub) {
				// printf("node.%lld send heartbeat\n", msg->id);

				// appendEntries
			} else {
				LOGD("unsupport msgType:%d", msg->msg_type);
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
		
		for (auto msg : todo)
		{
			if (msg->msg_type == msg_vote || msg->msg_type == msg_prop) {
				std::string rec;
				msg->encoder(rec);
				rn->wal.writeRecord(rec);
			}
		}

		int syncState = fsync(rn->wal.fd_);

		std::vector<RaftMsg*> todo2;
		for (auto msg : todo)
		{
			if (msg->id == rc->id) {
				todo2.push_back(msg);
			} else {
				if (msg->back)
					msg->back->cv.notify_one();
			}
		}

		if (todo2.size()>0) {
			rn->msg_node_mu.lock();
			rn->msg_node_vec.insert(rn->msg_node_vec.end(), todo2.begin(), todo2.end());
			int size = rn->msg_node_vec.size();
			rn->msg_node_mu.unlock();
			if (size == todo2.size())
				rn->msg_node_cv.notify_one();
		}
	}

}


// g++ myraft.cc  wal.cc timer.cc raftserviceimpl.cc proto/raftpb.grpc.pb.cc proto/raftpb.pb.cc -std=c++11 -o myraft -lprotobuf -lgrpc++

int main(int argc, char const *argv[])
{
	if (argc<3) {
		LOGD("need param %d",argc);
		return 1;
	}

	int id = atoi(argv[1]);
	if (id<=0) {
		LOGD("id err %s",argv[1]);
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
	
	LOGD("open wal...");
	std::string filename = std::string("./wal.") + std::string(argv[1]);
	int ret = rn->wal.openWal(filename.c_str(),es,hs);
	if (ret!=0) {
		std::cout << "open wal failed " << ret << std::endl;
		return 1;
	}


	int startId = 1;
	for (auto addr : peerAddrs)
	{
		auto c = new RaftClient(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials()));
		c->next_index = 0;
		rn->peers[startId] = c;
		startId++;
	}


	
	RaftCore* rc = new RaftCore(id);
	rc->term = hs.term;
	rc->voteFor = hs.voteFor;

	if (es.size()>0) {
		rc->lastLogTerm = es.back()->term;
		rc->lastLogIndex = es.back()->index;
	}

	// std::cout << "start raftcore:" << rc->term << " " << rc->voteFor << std::endl; 
	// std::cout << "start raftcore:" << es.size() << std::endl; 

	rc->election_timer = new Timer(rc->tm);
	rc->election_timer->start(startElection,2000, Timer::TimerType::ONCE,rc);
	
	std::thread sysmon_worker(sysmon,rc);
	std::thread wal_worker(startWalWorker,rn,rc);
	std::thread node_worker(startRaftNode,rn,rc);
	std::thread node_service(startRaftService,peerAddrs[id-1],rn,rc);


	// Timer rc_ticker(tm);
	// rc_ticker.start(tick,400, Timer::TimerType::CIRCLE,rc);



	sysmon_worker.join();
	node_worker.join();
	node_service.join();
	wal_worker.join();

	return 0;
}










