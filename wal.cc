#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <string>
#include <iostream>

#include "raft.h"

int Wal::openWal(const char* filename,std::vector<Entry*>& es,HardState& hs) 
{
	int fd = open(filename,O_RDWR|O_CREAT,0755);
	if (fd<=0)
		return 1;
	char* buf;
	int n;
	int32_t dataLength;
	int bufLen = 4096;
	char* data;
	int32_t msg_type;
	int offset = 0;

	buf = new char[bufLen];

	while (true) 
	{
		// seek

		if (lseek(fd,offset,SEEK_SET)<0)
			return 10000;

		n = read(fd,buf,bufLen);

		if (n<0)
			return 10001;

		if (n==0)
			break;

		if (n<12)
		{
			return 10002;
		}

		std::string headerMagic(buf,4);
		if (headerMagic != "aa55")
		{
			return 10003;
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
					return 10004;
				}
				Entry* e = new Entry();
				e->term = *((int64_t*)data);
				e->index = *((int64_t*)(data+8));
				e->record = std::string(data+8+8,dataLength-4-8-8);

				// 实现日志覆盖
				if (es.size()>0) {
					if (e->index == es.back()->index+1) {
					} else if (e->index>es.back()->index+1) {
						// 强制忽略掉前面的日志
						es.clear();
					} else {
						int offset = e->index-es.front()->index;
						if (offset<0)
							offset = 0;
						es.resize(offset);
					}
				}
				es.push_back(e);

				if (e->term>hs.term) {
					hs.term = e->term;
					hs.voteFor = 0;
				}
			} else if (msg_type == msg_vote) {
				if (dataLength<8+8) {
					return 10005;
				}
				hs.term = *((int64_t*)data);
				hs.voteFor = *((int64_t*)(data+8));
			} else {
				return 10006;
			}
		}

		offset += 4+4+dataLength;
	}

	this->fd_ = fd;
	return 0;
}

int Wal::writeRecord(std::string rec) 
{
	int n = write(this->fd_,rec.c_str(),rec.size());

	if (n != rec.size())
		return 1;
	return 0;
}