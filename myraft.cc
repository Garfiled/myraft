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


TimerManager tm;

int RaftCore::propose(Entry* e) {
	if (e==NULL)
		return -1;
	this->spinLock.lock();
	if (this->leader == 0) {
		this->spinLock.unlock();
		return -1;
	}
	if (this->state != Leader) {
		int l = this->leader;
		this->spinLock.unlock();
		return l;
	}
	
	this->push_entry(e);

	int size = this->messageVec.size();
	this->spinLock.unlock();
	if (size==1)
		this->msgCV.notify_one();
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
	rc->spinLock.lock();
	if (rc->state == Leader) {
		RaftMsg* m = new RaftMsg(msg_hub);
		m->term = rc->term;
		rc->messageVec.push_back(m);
	}
	int size = rc->messageVec.size();
	rc->spinLock.unlock();
	if (size==1)
	rc->msgCV.notify_one();
}

void startElection(void* arg) {
	std::cout << "startElection ..." << std::endl;
	RaftCore* rc =(RaftCore*)arg;
	RaftMsg* msg = new RaftMsg(msg_vote);
	rc->spinLock.lock();
	rc->state = Candidate;
	rc->leader = 0;
	rc->term++;
	rc->voteFor = rc->id;


	msg->id = rc->id;
	msg->term = rc->term;
	msg->log_term = rc->lastLogTerm;
	msg->log_index = rc->lastLogIndex;

	rc->messageVec.push_back(msg);
	int size = rc->messageVec.size();
	rc->spinLock.unlock();
	if (size==1)
		rc->msgCV.notify_one();
}


void RaftCore::push_entry(Entry* e)
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

	this->messageVec.push_back(m);
}

int RaftCore::vote_back(int happenTerm,bool win,int peerTerm) {
	int becomeLeader = 0;
	this->spinLock.lock();
	if (this->state == Candidate && this->term == happenTerm && win) {
		this->state = Leader;
		this->leader = id;

		// 生成一条noop entry 防止幽灵事件
		Entry* e = new Entry();
		this->push_entry(e);
		becomeLeader = 1;
	} else {
		this->state = Follower;
		this->leader = 0;
		this->voteFor = 0;
		if (this->term < peerTerm)
			this->term = peerTerm;
	}

	int size = this->messageVec.size();
	this->spinLock.unlock();
	if (size==1)
		this->msgCV.notify_one();

	return becomeLeader;
}

void RaftNode::reset(RaftCore* rc)
{
	Timer* rc_election_timer = new Timer(tm);
	rc_election_timer->start(startElection,1000, Timer::TimerType::ONCE,rc);
}

int RaftNode::handleVote(RaftCore* rc,RaftMsg* msg) 
{
	// write wal
	std::string voteRec;
	voteRec.append("aa55",4);
	int32_t dataLength = 4 + 8 + 8;
	voteRec.append((char*)&dataLength,4);
	int32_t voteMsg = msg_vote;
	voteRec.append((char*)&voteMsg,4);
	voteRec.append((char*)&msg->term,8);
	voteRec.append((char*)&msg->id,8);

	int err = this->wal.writeRecord(voteRec.c_str(),voteRec.size());
	if (err!=0)
		return err;

	fsync(this->wal.fd_);

	if (rc->id != msg->id) {
		if (msg->back)
			msg->back->cv.notify_one();
		return 0;
	}

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
			std::cout << "requestVote:" << s.error_code() << ": " << s.error_message() << std::endl;
		}
	}

	if (voteForCnt>this->peers.size()/2) {
		if (rc->vote_back(msg->term,true,maxTerm)>0) {
			printf("node.%lld become leader on term %lld\n",rc->id,msg->term);
		} else {
			this->reset(rc);	
		}
	} else {
		printf("node.%lld vote failed on term %lld\n cnt %d",rc->id,msg->term,voteForCnt);
		rc->vote_back(msg->term,false,maxTerm);
		this->reset(rc);
	}

	return 0;
}

int RaftNode::handleProp(RaftCore* rc,RaftMsg* msg) 
{
	// write wal
	for (auto e : msg->ents) {
		std::string propRec;
		propRec.append("aa55",4);
		int32_t dataLength = 4 + 8 + 8 + e->record.size();
		propRec.append((char*)&dataLength,4);
		int32_t propMsg = msg_prop;
		propRec.append((char*)&propMsg,4);
		propRec.append((char*)&e->term,8);
		propRec.append((char*)&e->index,8);
		propRec.append(e->record);

		this->wal.writeRecord(propRec.c_str(),propRec.size());
	}

	// fsync
	fsync(this->wal.fd_);


	if (msg->id != rc->id)
		if (msg->back)
			msg->back->cv.notify_one();
		return 0;

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
		// reqAE.entries.push_back(msg->ents);

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
			std::cout << "appendEntry:" << s.error_code() << ": " << s.error_message() << std::endl;
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
	printf("startRaftNode node.%lld\n", rc->id);
	while (true) 
	{
		{
			std::unique_lock<std::mutex> lk(rc->mu);
			rc->spinLock.lock();
			if (rc->messageVec.size()==0) {
				rc->spinLock.unlock();
				rc->msgCV.wait(lk);
				continue;
			}
		}

		std::vector<RaftMsg*> msgVec = rc->messageVec;
		rc->messageVec.clear();
		rc->spinLock.unlock();

		for (auto msg : msgVec)
		{
			if (msg->msg_type == msg_prop) {
				printf("node.%lld on term.%lld recv %lu\n",rc->id,rc->term,msg->ents.size());

				rn->handleProp(rc,msg);

			} else if (msg->msg_type == msg_vote) {
				printf("node.%lld start election on term %lld\n", msg->id,msg->term);
				rn->handleVote(rc,msg);
			} else if (msg->msg_type == msg_hub) {
				// printf("node.%lld send heartbeat\n", msg->id);

				// appendEntries
			} else {
				printf("unsupport msgType:%d\n", msg->msg_type);
			}

			delete msg;
		}


	}

}

void sysmon() 
{
	tm.detect_timers();
}


// g++ myraft.cc  wal.cc timer.cc raftserviceimpl.cc proto/raftpb.grpc.pb.cc proto/raftpb.pb.cc -std=c++11 -o myraft -lprotobuf -lgrpc++

int main(int argc, char const *argv[])
{
	if (argc<3) {
		std::cout << "need param " << argc << std::endl;
		return 1;
	}

	int id = atoi(argv[1]);
	if (id<=0) {
		std::cout << "id err " << argv[1] << std::endl;
		return 1;
	}
	std::vector<std::string> peerAddrs;
	for (int i=2;i<argc;i++) {
		peerAddrs.push_back(std::string(argv[i]));
	}


	RaftNode* rn = new RaftNode();

	// 初始化状态
	std::vector<Entry*> es;
	HardState hs = {0,0};
	
	std::cout << "open wal..." << std::endl;
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

	std::thread node_worker(startRaftNode,rn,rc);
	std::thread sysmon_worker(sysmon);
	std::thread node_service(startRaftService,peerAddrs[id-1]);


	// Timer rc_ticker(tm);
	// rc_ticker.start(tick,400, Timer::TimerType::CIRCLE,rc);

	rn->reset(rc);
	
	sysmon_worker.join();
	node_worker.join();
	node_service.join();

	return 0;
}










