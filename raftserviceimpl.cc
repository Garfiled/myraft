#include <string>
#include <grpcpp/grpcpp.h>


#include "raftserviceimpl.h"
#include "proto/raftpb.grpc.pb.h"
#include "raft.h"

grpc::Status RaftServiceImpl::RequestVote(grpc::ServerContext* context,const raftpb::ReqVote* req,raftpb::RespVote* reply)
{
    bool voteGranted = false;
    int term = 0;
    MsgBack* mb = nullptr;

    this->rc->spinLock.lock();
    if (rc->state == Follower && (rc->voteFor==0 || rc->voteFor == req->candidateid()) && req->term()>=rc->term 
        && req->lastlogterm()>=rc->lastLogTerm && req->lastlogindex()>=rc->lastLogIndex) {
        // reset timer

        rc->voteFor = req->candidateid();
        rc->term = req->term();
        rc->leader = 0;

        auto msg = new RaftMsg(msg_vote);
        msg->id = req->candidateid();
        msg->term = req->term();

        mb = new MsgBack();
        msg->back = mb;

        rc->messageVec.push_back(msg);

    } else {
        printf("node.%lld term.%lld refuse vote c.id %u c.term %lld\n",rc->id,rc->term,req->candidateid(),req->term());
    }
    term = rc->term;
    this->rc->spinLock.unlock();


    // wait for
    if (mb!=nullptr) {
        std::unique_lock<std::mutex> lk(mb->mu);
        mb->cv.wait(lk);
        if (mb->err==0) {
            voteGranted = true;
        }
        delete mb;
    }

    reply->set_term(term);
    reply->set_votegranted(voteGranted);

    return grpc::Status::OK;

}

grpc::Status RaftServiceImpl::AppendEntries(grpc::ServerContext* context,const raftpb::ReqAppendEntry* req,raftpb::RespAppendEntry* reply)
{
    int term = 0;
    bool success = false;
    MsgBack* mb = nullptr;

    this->rc->spinLock.lock();
    if (rc->term > req->term())
        term = rc->term;
    else {
        // 信任这条消息
        rc->term = req->term();
        rc->leader = req->leaderid();
        rc->state = Follower;

        // reset election timeout

        if (rc->lastLogTerm == req->prevlogterm() && rc->lastLogIndex==req->prevlogindex()) {
            if (req->entries().size()>0) {              
                    RaftMsg* msg = new RaftMsg(msg_prop);
                    for (auto re : req->entries()) {
                        Entry* e = new Entry();
                        e->term = re.term();
                        e->index = re.index();
                        e->record = re.record();

                        msg->ents.push_back(e);

                        // 先放入raftcore中，后续应该要独立出来，并且由node来负责维护
                        rc->ents.push_back(e);
                    }
                    mb = new MsgBack();
                    msg->back = mb;         

                    rc->lastLogTerm = msg->ents.back()->term;
                    rc->lastLogIndex = msg->ents.back()->index;

                    rc->messageVec.push_back(msg);
                    
            } else {
                success = true;
            }
        } else {
            if (req->prevlogindex()>rc->lastLogIndex) {

            } else {
                int eraseNum = 0;
                if (req->prevlogindex()==0) {
                    eraseNum = rc->lastLogIndex;
                } else {
                    eraseNum = rc->lastLogIndex-req->prevlogindex()+1;
                }

                rc->ents.resize(rc->ents.size()-eraseNum);

                if (rc->ents.size()>0) {
                    rc->lastLogTerm = rc->ents.back()->term;
                    rc->lastLogIndex = rc->ents.back()->index;
                } else {
                    rc->lastLogTerm = 0;
                    rc->lastLogIndex = 0;
                }
            }
        }
    }
    term = rc->term;
    this->rc->spinLock.unlock();

    if (mb != nullptr) {
        std::unique_lock<std::mutex> lk(mb->mu);
        mb->cv.wait(lk);
        if (mb->err==0) {
            success = true;
        }
        delete mb;
    }

    reply->set_term(term);
    reply->set_success(success);

    return grpc::Status::OK;
}

grpc::Status RaftServiceImpl::Tran(grpc::ServerContext* context,const raftpb::ReqTran* req,raftpb::RespTran* reply)
{
    return grpc::Status::OK;
}


void startRaftService(std::string address) {
    std::cout << "startRaftService:" << address << std::endl;
    RaftServiceImpl service;

    grpc::ServerBuilder builder;

    builder.AddListeningPort(address, grpc::InsecureServerCredentials());

    builder.RegisterService(&service);


    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    server->Wait();
}
