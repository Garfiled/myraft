#pragma once

#include <string>
#include <vector>
#include <map>
#include <atomic>

#include "raftserviceimpl.h"

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
	RaftMsg(RaftMsgType mt):msg_type(mt) {};
	RaftMsgType msg_type;
	uint64_t id;
	uint64_t term;
	uint64_t log_term;
	uint64_t log_index;
	std::vector<Entry*> ents;
	MsgBack* back;
};


struct HardState {
	uint64_t term;
	uint64_t voteFor;
};

class Wal {
public:
	int fd_;

	int openWal(const char* filename,std::vector<Entry*>&,HardState&);
	int writeRecord(const char*,int);
};


class RaftCore
{
public:
	RaftCore(int);
	~RaftCore();

	void tick();
	int propose(Entry* e);
	int vote_back(int,bool,int);
	void push_entry(Entry* e);

	int64_t id;
	RaftState state;
	int64_t leader;
	int64_t term;
	int voteFor;
	int lastLogTerm;
	int lastLogIndex;

	std::vector<Entry*> ents;

	SpinLock spinLock;
	std::condition_variable cv;

	std::mutex mu;
	std::condition_variable msgCV;
	std::vector<RaftMsg*> messageVec;
};


class RaftNode {
public:
	Wal wal;
	std::map<int,RaftClient*> peers;

	int handleVote(RaftCore*,RaftMsg*);
	int handleProp(RaftCore*,RaftMsg*);
	void reset(RaftCore*);
};
