#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <unistd.h>

#include "timer.h"
#include "raft.h"


class RaftCore;


TimerManager tm;
RaftCore* rc;


struct RaftMsg {
	RaftMsgType msgType;
	int64_t id;
	int64_t term;
	int64_t logTerm;
	int64_t logIndex;
	std::vector<Entry*> entries;
};

enum RaftState {Follower = 0,Candidate = 1,Leader = 2};

class SpinLock {
    std::atomic_flag locked = ATOMIC_FLAG_INIT ;
public:
    void lock() {
        while (locked.test_and_set(std::memory_order_acquire)) { ; }
    }
    void unlock() {
        locked.clear(std::memory_order_release);
    }
};

class RaftCore
{
public:
	RaftCore(int);
	~RaftCore();

	void tick();
	int propose(Entry* e);
	void vote_back(int,bool);

	int64_t id;
	RaftState state;
	int leader;
	int64_t term;
	int voteFor;
	int lastLogTerm;
	int lastLogIndex;
	RaftMsg* lastMsg;

	// 这里最好可以实现先自旋一阵再进行等待，因为对RaftCore的处理理论上都是很短的操作
	SpinLock spinLock;

	std::vector<RaftMsg*> messageVec;
};

class RaftNode {
public:
	Wal wal;
};

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
		

	e->term = this->term;
	e->index = this->lastLogIndex+1; 
	
	this->lastLogIndex++;

	// entry batch
	if (this->lastMsg!=NULL && (this->lastMsg->msgType == msg_prop ||this->lastMsg->msgType == msg_hub)) {
		this->lastMsg->msgType = msg_prop;
		this->lastMsg->entries.push_back(e);
	} else {
		RaftMsg* m = new RaftMsg();
		m->msgType = msg_prop;
		this->messageVec.push_back(m);
		this->lastMsg = m;
	}

	this->spinLock.unlock();
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
	this->lastMsg = NULL;
}

void tick(void* arg) {
	RaftCore* rc =(RaftCore*)arg;
	rc->spinLock.lock();
	if (rc->state == Leader) {
		RaftMsg* m = new RaftMsg();
		m->msgType = msg_hub;
		m->term = rc->term;
		rc->messageVec.push_back(m);
	}
	rc->spinLock.unlock();
}

void startElection(void* arg) {
	RaftCore* rc =(RaftCore*)arg;
	rc->spinLock.lock();
	rc->state = Candidate;
	rc->leader = 0;
	rc->term++;
	rc->voteFor = rc->id;


	RaftMsg* msg = new RaftMsg();
	msg->id = rc->id;
	msg->msgType = msg_vote;
	msg->term = rc->term;
	msg->logTerm = rc->lastLogTerm;
	msg->logIndex = rc->lastLogIndex;
	rc->messageVec.push_back(msg);
	rc->spinLock.unlock();
}

void RaftCore::vote_back(int term,bool win) {
	this->spinLock.lock();
	if (this->term > term) {
	} else if (this->term == term) {
		if (win) {
			this->state = Leader;
			this->leader = id;
			printf("node.%lld become leader on term %lld\n",this->id,this->term);
		} else {
			this->state = Follower;
			this->leader = 0;
			this->voteFor = 0;
		}		
	} else {
		this->state = Follower;
		this->leader = 0;
		this->voteFor = 0;
	}

	// 生成一条noop entry



	Entry* e = new Entry();
	e->term = this->term;
	e->index = this->lastLogIndex+1; 
	
	this->lastLogTerm = this->term;
	this->lastLogIndex = this->lastLogIndex+1;

	// entry batch
	if (this->lastMsg!=NULL && (this->lastMsg->msgType == msg_prop ||this->lastMsg->msgType == msg_hub)) {
		this->lastMsg->msgType = msg_prop;
		this->lastMsg->entries.push_back(e);
	} else {
		RaftMsg* m = new RaftMsg();
		m->msgType = msg_prop;
		m->entries.push_back(e);
		this->messageVec.push_back(m);
		this->lastMsg = m;
	}

	this->spinLock.unlock();
}

void startRaftNode(RaftNode* rn,RaftCore* rc) 
{
	printf("startRaftNode node.%lld\n", rc->id);
	while (true) 
	{
		rc->spinLock.lock();
		if (rc->messageVec.size()==0) {
			rc->spinLock.unlock();
			continue;
		}

		std::vector<RaftMsg*> msgVec = rc->messageVec;
		rc->messageVec.clear();
		rc->spinLock.unlock();

		for (auto msg : msgVec)
		{
			if (msg->msgType == msg_prop) {
				printf("node.%lld on term.%lld recv %lu\n",rc->id,rc->term,msg->entries.size());

				// write wal
				for (auto e : msg->entries) {
					std::string propRec;
					propRec.append("aa55",4);
					int32_t dataLength = 4 + 8 + 8 + e->record.size();
					propRec.append((char*)&dataLength,4);
					int32_t propMsg = msg_prop;
					propRec.append((char*)&propMsg,4);
					propRec.append((char*)&e->term,8);
					propRec.append((char*)&e->index,8);
					propRec.append(e->record);

					rn->wal.writeWal(propRec.c_str(),propRec.size());
				}


				// appendEntries

				// write mem store



			} else if (msg->msgType == msg_vote) {
				printf("node.%lld start election on term %lld\n", msg->id,msg->term);

				std::string voteRec;
				voteRec.append("aa55",4);
				int32_t dataLength = 4 + 8 + 8;
				voteRec.append((char*)&dataLength,4);
				int32_t voteMsg = msg_vote;
				voteRec.append((char*)&voteMsg,4);
				voteRec.append((char*)&msg->term,8);
				voteRec.append((char*)&msg->id,8);
				// write wal
				rn->wal.writeWal(voteRec.c_str(),voteRec.size());
				
				// requestVote

				rc->vote_back(msg->term,true);

				// Timer rc_election_timer(tm);
				// rc_election_timer.start(startElection,1000, Timer::TimerType::ONCE);
			} else if (msg->msgType == msg_hub) {
				// printf("node.%lld send heartbeat\n", msg->id);

				// appendEntries
			} else if (msg->msgType == msg_append){

				// write wal
				printf("unsupport msgType:%d\n", msg->msgType);
			}

			delete msg;
		}


	}

}

void sysmon() 
{
	tm.detect_timers();
}


int main(int argc, char const *argv[])
{

	// std::thread sysmon_worker(sysmon);

	RaftNode* rn = new RaftNode();
	std::vector<Entry*> es;
	int64_t term;
	int64_t voteFor;
	std::cout << "open wal..." << std::endl;
	int ret = rn->wal.openWal("./wal",es,&term,&voteFor);
	if (ret!=0) {
		std::cout << "open wal failed " << ret << std::endl;
		return 1;
	}
	
	rc = new RaftCore(1);
	rc->term = term;
	rc->voteFor = voteFor;

	if (es.size()>0) {
		rc->lastLogTerm = es.back()->term;
		rc->lastLogIndex = es.back()->index;
	}

	// std::cout << "start raftcore:" << rc->term << " " << rc->voteFor << std::endl; 
	// std::cout << "start raftcore:" << es.size() << std::endl; 

	std::thread node_worker(startRaftNode,rn,rc);



	Timer rc_ticker(tm);
	rc_ticker.start(tick,400, Timer::TimerType::CIRCLE,rc);

	Timer rc_election_timer(tm);
	rc_election_timer.start(startElection,1000, Timer::TimerType::ONCE,rc);

	std::thread sysmon_worker(sysmon);

	




	// Entry* e = new Entry();
	// e->record = "hello world!";
	// int ret = rc->propose(e);
	// printf("%d\n", ret);
	// sleep(1);

	
	sysmon_worker.join();
	node_worker.join();

	return 0;
}










