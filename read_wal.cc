#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <iostream>

#include "raft.h"




int main() 
{
	int fd = open("./wal",O_RDONLY);
	if (fd<=0) {
		std::cout << "open wal failed:" << fd << std::endl;
		return 1;
	}
	char* buf;
	int n;
	int32_t dataLength;
	int bufLen = 4096;
	char* data;
	int32_t msg_type;
	int offset = 0;

	std::vector<Entry*> es;
	int term;
	int voteFor;

	buf = new char[bufLen];

	std::string magic("aa55",4);

	while (true) 
	{
		// seek

		lseek(fd,offset,SEEK_SET);

		n = read(fd,buf,bufLen);

		// std::cout << n << " " << offset << std::endl;

		if (n<=0)
			break;

		if (n<12)
		{
			std::cout << "header len err " << n << std::endl;
			return 1;
		}

		std::string headerMagic(buf,4);
		if (headerMagic != magic)
		{
			std::cout << "header magic err " << headerMagic << std::endl;
			return 1;
		}

		dataLength = *((int32_t*)(buf+4));

		// std::cout << "dataLength:" << dataLength << std::endl;

		if (dataLength+4+4>n) {
			delete buf;
			buf = new char[dataLength+4+4];
			bufLen = dataLength+4+4;

			continue;
		} else {
			data = buf+4+4;
			msg_type = *((int32_t*)data);
			data += 4;
			if (msg_type == msg_entry) {
				if (dataLength<4+8+8)
				{
					std::cout << "entry len err " << dataLength << std::endl;
					return 1;
				}
				Entry* e = new Entry();
				e->term = *((int64_t*)data);
				e->index = *((int64_t*)(data+8));
				e->record = std::string(data+8+8,dataLength-4-8-8);
				es.push_back(e);

				std::cout << "entry: " << e->term << " " << e->index << " " << e->record << std::endl;
			} else if (msg_type == msg_vote) {
				if (dataLength<8+8) {
					std::cout << "vote len err " << dataLength << std::endl;
					return 1;
				}
				term = *((int64_t*)data);
				voteFor = *((int64_t*)(data+8));

				std::cout << "vote: " << term << " " << voteFor << std::endl;
			} else {
				std::cout << "unsupport type err " << msg_type << std::endl;
				return 1;
			}
		}

		offset += 4+4+dataLength;
		// std::cout << "offset:" << offset << std::endl;
	}

	return 0;
}
