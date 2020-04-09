#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <thread>
#include <poll.h>
#include <sys/epoll.h>
#include <string>
#include <iostream>
#include <fstream>
#include <utility>

#include "http-parser.h"
#include "logutils.h"
#include "raft.h"

#define MAX_EVENTS 500

class HttpServer
{
public:
    int socket_fd;
    RaftCore* rc;
};

HttpServer* server;

void worker(int);
int acceptConn(int socket_fd,int epoll_fd);
void sendHttpObj(HttpRequest&,std::string);
void handleHttp(HttpRequest& req);

int startHttpWorker(int port,int thread_num,RaftCore* rc_)
{

    int socket_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        LOGE("socket failed");
        return -1;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address))<0)
    {
        LOGE("bind failed");
        return -2;
    }
    if (listen(socket_fd, 5) < 0)
    {
        return -3;
    }

    server = new HttpServer();
    server->socket_fd = socket_fd;
    server->rc = rc_;

    // 创建线程池
    for (int i=0;i<thread_num;i++)
    {
	   auto w = new std::thread(worker,socket_fd);
    }
    return 0;
}

void worker(int socket_fd)
{
    // 创建epoll实例
    int epoll_fd = epoll_create(MAX_EVENTS);
    struct epoll_event event;
    struct epoll_event eventList[MAX_EVENTS];
    event.events = EPOLLIN|EPOLLET;
    event.data.fd = socket_fd;

    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event) < 0)
    {
        LOGI("epoll add failed");
        return;
    }

    std::map<int,Session> workerSess;
    //epoll
    while(1)
    {
        int eventCnt = epoll_wait(epoll_fd, eventList, MAX_EVENTS, 3000);
        if(eventCnt < 0) {
            LOGI("epoll error ")
            break;
        } else if (eventCnt == 0) {
            continue;
        }
        for(int i=0; i<eventCnt; i++)
        {
            if ((eventList[i].events & EPOLLERR) || (eventList[i].events & EPOLLHUP) || !(eventList[i].events & EPOLLIN)) {
                LOGI("epoll event error");
                close (eventList[i].data.fd);
                continue;
            }

            if (eventList[i].data.fd == socket_fd) {
                int ret = acceptConn(socket_fd,epoll_fd);
                if (ret>0) {
                    workerSess[ret] = Session(ret,4096);
                }
            } else {
                if (workerSess.end() == workerSess.find(eventList[i].data.fd))
                {
                    LOGI("session not found ");
                    continue;
                }

                int ret = processQuery(workerSess[eventList[i].data.fd]);
                if (ret!=0)
                {
                    close(eventList[i].data.fd);
                    delete(workerSess[eventList[i].data.fd].buf);
                    workerSess.erase(eventList[i].data.fd);
                }
            }
        }
    }

    close(epoll_fd);

}

void handleHttp(HttpRequest& req)
{
    int ret = 0;
    if (req.method=="GET")
    {

    } else if (req.method == "POST") {
        auto e = new Entry();
        e->record = req.body;
        ret = server->rc->propose(e);
    }
    if (ret==0)
        sendHttpObj(req,"ok");
    else
        sendHttpObj(req,"handleHttp failed");
}

void sendHttpObj(HttpRequest& req,std::string o)
{
    std::string ret;
    ret.append(req.version);
    ret.append(" 200 OK\r\n",9);
    ret.append("Date: ",6);
    char buf[50];
    time_t now = time(0);
    struct tm tm = *localtime(&now);
    strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    ret.append(buf,strlen(buf));
    ret.append("\r\n",2);

    ret.append("Content-Length: ",16);
    ret.append(std::to_string(o.size()));
    ret.append("\r\n",2);
    ret.append("Content-Type: text/plain; charset=utf-8\r\n",41);
    if (req.version=="HTTP/1.0" && req.header["Connection"] == "Keep-Alive") {
        ret.append("Connection: keep-alive\r\n\r\n",26);
    } else {
        ret.append("\r\n",2);
    }
    ret.append(o);

    // std::cout << ret << std::endl;
    ::send(req.fd,ret.c_str(),ret.size(),0);
}

int acceptConn(int socket_fd,int epoll_fd)
{
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    bzero(&address, addrlen);

    int client_fd = accept(socket_fd, (struct sockaddr *) &address, &addrlen);

    if (client_fd < 0) {
        LOGI("accept error ");
        return client_fd;
    }
    //将新建立的连接添加到EPOLL的监听中
    struct epoll_event event;
    event.data.fd = client_fd;
    event.events = EPOLLIN;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);

    return client_fd;
}

