#pragma once

#include <map>
#include <string>

#define CONTENT_LEN_LIMIT 500*1024*1024

#define ERR_HTTP_NOT_COMPLETE 100001
#define ERR_HTTP_REQ_METHOD   100002
#define ERR_HTTP_REQ_URI      100003
#define ERR_HTTP_REQ_VERSION  100004
#define ERR_HTTP_HEADER_KV    100005
#define ERR_HTTP_HEADER_CONTENT_LENGTH    100006
#define ERR_HTTP_CONTENT_LIMIT            100007
#define ERR_HTTP_CONNECT_CLOSE  100008
#define ERR_HTTP_READ_EOF 100009

class Session
{
public:
    Session(int _fd,int size)
    {
        fd = _fd;
        buf = new char[size];
        cap = size;
        len = 0;
        start = 0;
    }
    Session():fd(0),buf(nullptr),len(0),cap(0),start(0){};
    int fd;
    char* buf;
    int len;
    int cap;
    int start;
};

class HttpRequest
{
public:
    std::string method;
    std::string uri;
    std::string version;
    std::map<std::string,std::string> header;
    std::string body;

    int fd;
};

int processQuery(Session&);