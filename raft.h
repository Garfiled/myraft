#pragma once

#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <time.h>

#include "raftserviceimpl.h"
#include "timer.h"

class RaftCore;
class RaftNode;


enum RaftMsgType {msg_prop = 1,msg_hub = 2,msg_vote = 3};
enum RaftState {Follower = 0,Candidate = 1,Leader = 2};


class SpinLock 
{
    std::atomic_flag locked = ATOMIC_FLAG_INIT ;
public:
    void lock() {
        while (locked.test_and_set(std::memory_order_acquire)) { ; }
    }
    void unlock() {
        locked.clear(std::memory_order_release);
    }
};

struct Entry {
	int term;
	int index;
	std::string record;
};

class MsgBack {
public:
	MsgBack():err(0){};
	int err;
	std::mutex mu;
	std::condition_variable cv;
};

class RaftMsg {
public:
	RaftMsg(RaftMsgType mt):msg_type(mt),back(nullptr) {};
	int encoder(std::string& rec) 
	{
		if (msg_type==msg_vote) {
			rec.append("aa55",4);
			int32_t dataLength = 4 + 8 + 8;
			rec.append((char*)&dataLength,4);
			int32_t voteMsg = msg_vote;
			rec.append((char*)&voteMsg,4);
			rec.append((char*)&term,8);
			rec.append((char*)&id,8);
		} else if (msg_type == msg_prop){
			for (auto e : ents) {
				rec.append("aa55",4);
				int32_t dataLength = 4 + 8 + 8 + e->record.size();
				rec.append((char*)&dataLength,4);
				int32_t propMsg = msg_prop;
				rec.append((char*)&propMsg,4);
				rec.append((char*)&e->term,8);
				rec.append((char*)&e->index,8);
				rec.append(e->record);
			}
		} else {
			return 1;
		}

		return 0;
	}
	RaftMsgType msg_type;
	uint64_t id;
	uint64_t term;
	uint64_t log_term;
	uint64_t log_index;
	std::vector<Entry*> ents;
	void* callback_arg;
	void (*callback)(void*,int);
	MsgBack* back;
	int fd;
};


struct HardState {
	uint64_t term;
	uint64_t voteFor;
};

class Wal {
public:
	int fd_;

	int openWal(const char* filename,std::vector<Entry*>&,HardState&);
	int writeRecord(std::string);
};


class RaftCore
{
public:
	RaftCore(int);
	~RaftCore();

	void tick();
	int propose(Entry*,void (*cb)(void*,int),void*);
	int vote_back(int,bool,int);
	RaftMsg* makePropMsg(Entry*);
	void resetElectionTimer();
	void resetHeartBeatTick();

	int64_t id;
	RaftState state;
	int64_t leader;
	int64_t term;
	int voteFor;
	int lastLogTerm;
	int lastLogIndex;

	std::vector<Entry*> ents;

	std::mutex mu;

	std::vector<RaftMsg*> msg_wal_vec;

	std::condition_variable msg_wal_cv;

	Timer* election_timer;
	Timer* hub_ticker;

	TimerManager tm;
};


class RaftNode {
public:
	Wal wal;
	std::map<int,RaftClient*> peers;

	std::vector<RaftMsg*> msg_node_vec;
	std::mutex msg_node_mu;
	std::condition_variable msg_node_cv;

	int handleVote(RaftCore*,RaftMsg*);
	int handleProp(RaftCore*,RaftMsg*);
	int handleHub(RaftCore*,RaftMsg*);
	void reset(RaftCore*);
};
