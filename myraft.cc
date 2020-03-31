#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <unistd.h>

#include "timer.h"


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

class RaftCore;

std::mutex mu;
std::condition_variable cv;
TimerManager tm;
RaftCore* rc;

struct Entry {
	int term;
	int index;
	char* record;
};

enum RaftMsgType {msg_entry = 1,msg_hub = 2,msg_vote = 3};

struct RaftMsg {
	RaftMsgType msgType;
	int id;
	int term;
	int logTerm;
	int logIndex;
	std::vector<Entry*> entries;
};

enum RaftState {Follower = 0,Candidate = 1,Leader = 2};

class RaftCore
{
public:
	RaftCore(int);
	~RaftCore();

	void tick();
	void startElection();
	int propose(Entry* e);
	void vote_back(int,bool);

	int id;
	RaftState state;
	int leader;
	int term;
	int voteFor;
	int lastLogTerm;
	int lastLogIndex;
	std::vector<RaftMsg*> messageVec;
};

int RaftCore::propose(Entry* e) {
	mu.lock();
	if (this->leader == 0) {
		mu.unlock();
		return -1;
	}
	if (this->state != Leader) {
		int l = this->leader;
		mu.unlock();
		return l;
	}
		

	e->term = this->term;
	e->index = this->lastLogIndex+1; 
	
	this->lastLogIndex++;

	RaftMsg* m = new RaftMsg();
	m->msgType = msg_entry;
	m->entries.push_back(e);
	this->messageVec.push_back(m);
	mu.unlock();
	cv.notify_one();
	return 0;
}

RaftCore::RaftCore(int _id) {
	this->id = _id;
}

void tick() {
	mu.lock();
	if (rc->state == Leader) {
		RaftMsg* m = new RaftMsg();
		m->msgType = msg_hub;
		m->term = rc->term;
		rc->messageVec.push_back(m);
		mu.unlock();
		cv.notify_one();
		return;
	}
	mu.unlock();
}

void startElection() {
	printf("startElection\n");
	mu.lock();
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
	mu.unlock();
	cv.notify_one();
}

void RaftCore::vote_back(int term,bool win) {
	mu.lock();
	if (this->term > term) {
	} else if (this->term == term) {
		if (win) {
			this->state = Leader;
			this->leader = id;
			printf("node.%d become leader on term %d\n",this->id,this->term);
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

	mu.unlock();

}

void startRaftNode(RaftCore* rc) 
{
	printf("startRaftNode node.%d\n", rc->id);
	while (true) 
	{
		std::unique_lock<std::mutex> lk(mu);
		if (rc->messageVec.size()==0) {
			cv.wait(lk);
			continue;
		}

		std::vector<RaftMsg*> msgVec = rc->messageVec;
		rc->messageVec.clear();
		lk.unlock();
		cv.notify_one();		

		for (auto msg : msgVec)
		{
			if (msg->msgType == msg_entry) {
				printf("node.%d on term.%d recv %lu\n",rc->id,rc->term,msg->entries.size());

			} else if (msg->msgType == msg_vote) {
				printf("node.%d start election on term %d\n", msg->id,msg->term);
				
				rc->vote_back(msg->term,true);

				// Timer rc_election_timer(tm);
				// rc_election_timer.start(startElection,1000, Timer::TimerType::ONCE);
			} else if (msg->msgType == msg_hub) {
				printf("node.%d send heartbeat\n", msg->id);
			} else {
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

	std::thread sysmon_worker(sysmon);

	
	rc = new RaftCore(1);

	std::thread node_worker(startRaftNode,rc);

	
	// Timer rc_ticker(tm);
	// rc_ticker.start(tick,400, Timer::TimerType::CIRCLE);

	Timer rc_election_timer(tm);
	rc_election_timer.start(startElection,1000, Timer::TimerType::ONCE);



	// Entry* e = new Entry();
	// e->record = "hello world!";
	// int ret = rc->propose(e);
	// printf("%d\n", ret);
	// sleep(1);

	
	sysmon_worker.join();
	// node_worker.join();

	return 0;
}










