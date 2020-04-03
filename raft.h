#include <string>
#include <vector>

struct Entry {
	int term;
	int index;
	std::string record;
};

enum RaftMsgType {msg_prop = 1,msg_hub = 2,msg_vote = 3, msg_append = 4};

class Wal {
public:
	int fd;

	int openWal(const char* filename,std::vector<Entry*>& es,int64_t* term,int64_t* voteFor);
	int writeWal(const char*,int);
};