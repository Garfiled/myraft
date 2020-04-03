#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <string>
#include <iostream>

#include "raft.h"

int Wal::openWal(const char* filename,std::vector<Entry*>& es,int64_t* term,int64_t* voteFor) 
{
	fd = open(filename,O_RDWR|O_CREAT,0755);

	char* buf;
	int n;
	int32_t dataLength;
	int bufLen = 4096;
	char* data;
	int32_t msg_type;
	int offset = 0;
	std::string magic("aa55",4);

	buf = new char[bufLen];

	while (true) 
	{
		// seek

		lseek(fd,offset,SEEK_SET);

		n = read(fd,buf,bufLen);

		if (n<=0)
			break;

		if (n<12)
		{
			return 10001;
		}

		std::string headerMagic(buf,4);
		if (headerMagic != magic)
		{
			return 10002;
		}

		dataLength = *((int32_t*)(buf+4));


		if (dataLength+4+4>n) {
			delete buf;
			buf = new char[dataLength+4+4];
			bufLen = dataLength+4+4;
			continue;
		} else {
			data = buf+4+4;
			msg_type = *((int32_t*)data);
			data += 4;
			if (msg_type == msg_prop) {
				if (dataLength<4+8+8)
				{
					return 10003;
				}
				Entry* e = new Entry();
				e->term = *((int64_t*)data);
				e->index = *((int64_t*)(data+8));
				e->record = std::string(data+8+8,dataLength-4-8-8);
				es.push_back(e);
			} else if (msg_type == msg_vote) {
				if (dataLength<8+8) {
					return 10004;
				}
				*term = *((int64_t*)data);
				*voteFor = *((int64_t*)(data+8));
			} else {
				return 10005;
			}
		}

		offset += 4+4+dataLength;
	}

	return 0;
}

int Wal::writeWal(const char* rec,int size) 
{
	int n = write(this->fd,rec,size);

	if (n != size)
		return 1;
	return 0;
}