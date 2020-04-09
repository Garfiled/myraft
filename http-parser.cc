#include <iostream>
#include <string>
#include <map>
#include <unistd.h>
#include <string.h>

#include "http-parser.h"

// 解析http协议
int parseHttp(char* buf,int* start_p,int length,HttpRequest& req);
int findHeaderEnd(char* buf,int start,int end);
int findLine(char* buf,int start,int end);
int findSpace(char* buf,int start,int end);
int findSub(char* buf,int start,int end);

int myAtoi(char* p,int end,int* val);

extern void handleHttp(HttpRequest& req);

int processQuery(Session& sess)
{
    int n = read(sess.fd,sess.buf+sess.len,sess.cap - sess.len);
    // std::cout << "processQuery:" << n << " " << sess.cap << std::endl;
    if (n<0)
    {
        return -1;
    } else if (n==0) {
        return ERR_HTTP_READ_EOF;
    }

    sess.len += n;

    int start=0;
    HttpRequest req;
    req.fd = sess.fd;
    int ret = parseHttp(sess.buf,&start,sess.len,req);

    if (ret == 0)
    {
        sess.start = start;
        if (start >= sess.len)
        {
            sess.len = 0;
            sess.start = 0;
        }

        // 命令解析完成，可以交给worker线程处理，这里暂时本地处理
        handleHttp(req);

        if (req.version == "HTTP/1.0" && req.header["Connection"] != "Keep-Alive") {
            close(req.fd);
            return ERR_HTTP_CONNECT_CLOSE;
        }
        return 0;

    } else if (ret==ERR_HTTP_NOT_COMPLETE)
    {
        if (sess.len>=sess.cap)
        {
            if (sess.len<CONTENT_LEN_LIMIT)
            {
                char* newBuf = new char[2*sess.cap];
                memcpy(newBuf,sess.buf+sess.start,sess.cap-sess.start);
                delete sess.buf;
                sess.buf = newBuf;
                sess.cap = 2*sess.cap;
                sess.len = sess.len-sess.start;
                sess.start = 0;
            } else {
                return ERR_HTTP_CONTENT_LIMIT;
            }
        }
    } else {
        return ret;
    }

    return 0;
}


int parseHttp(char* buf,int* start_p,int length,HttpRequest& req)
{
    int start = *start_p;
    int headerLen = findHeaderEnd(buf,start,length);
    if (headerLen<0)
        return ERR_HTTP_NOT_COMPLETE;
    int headerEnd = start+headerLen;
    int lineLen = findLine(buf,start,headerEnd+2);
    int lineEnd = start + lineLen;
    int reqMethodLen = findSpace(buf,start,lineEnd);
    if (reqMethodLen<=0)
        return ERR_HTTP_REQ_METHOD;
    req.method.append(buf+start,reqMethodLen);
    start += reqMethodLen+1;
    int reqURILen = findSpace(buf,start,lineEnd);
    if (reqURILen<=0)
        return ERR_HTTP_REQ_URI;
    req.uri.append(buf+start,reqURILen);
    start += reqURILen+1;
    if (start>=lineEnd)
        return ERR_HTTP_REQ_VERSION;
    req.version.append(buf+start,lineEnd-start);

   // std::cout << req.method << " " << req.uri << " " << req.version << std::endl;

    if (lineEnd == headerEnd)
    {
        *start_p = lineEnd+4;
        return 0;
    }
    start = lineEnd+2;

    while (start<headerEnd+2)
    {
        int lineLen = findLine(buf,start,headerEnd+2);
        int lineEnd = start + lineLen;
        int headerKeyLen = findSub(buf,start,lineEnd);
        if (headerKeyLen<0)
            return ERR_HTTP_HEADER_KV;
        std::string header_key(buf+start,headerKeyLen);
        std::string header_val(buf+start+headerKeyLen+2,lineLen-headerKeyLen-2);
        // std::cout << "header: " << header_key << " " << header_val << std::endl; 
        req.header.insert(std::pair<std::string,std::string>(header_key,header_val));

        start = lineEnd+2;
    }

    start += 2;

    // 需要继续解析Body数据
    if (req.method == "POST" && req.header.find("Content-Length")!=req.header.end())
    {
        std::string contentLengthStr = req.header["Content-Length"];
        int contentLength;
        int ret = myAtoi((char*)contentLengthStr.c_str(),contentLengthStr.size(),&contentLength);
        if (ret<0 || contentLength<0)
            return ERR_HTTP_HEADER_CONTENT_LENGTH;

        if (length-start>=contentLength)
        {
            req.body.append(buf+start,contentLength);
            start += contentLength;
        } else {
            return ERR_HTTP_NOT_COMPLETE;
        }
    }

    *start_p = start;
    return 0;
}

int findHeaderEnd(char* buf,int start,int end)
{
    for (int i = start;i<end-3;i++)
    {
        if (buf[i]=='\r' && buf[i+1]=='\n' && buf[i+2] == '\r' && buf[i+3] == '\n')
        {
            return i-start;
        }
    }
    return -1;
}

int findLine(char* buf,int start,int end)
{
    for (int i = start;i<end-1;i++)
    {
        if (buf[i]=='\r' && buf[i+1]=='\n')
        {
            return i-start;
        }
    }
    return -1;
}

int findSpace(char* buf,int start,int end)
{
    for (int i = start;i<end;i++)
    {
        if (buf[i]==' ')
        {
            return i-start;
        }
    }
    return -1;
}

int findSub(char* buf,int start,int end)
{
    for (int i = start;i<end;i++)
    {
        if (buf[i]==':')
        {
            return i-start;
        }
    }
    return -1;
}

int myAtoi(char* p,int end,int* val)
{
    int value= 0;
    int sign = 1;
    if (end<=0)
        return -1;
    for (int i=0;i<end;i++)
    {
        if (p[i]=='-')
            sign = -1;
        else if (p[i]<'0' || p[i]>'9')
            return -2;
        else
            value = value * 10 + p[i]-'0';
    }
    *val = value;
    return 0;
}
