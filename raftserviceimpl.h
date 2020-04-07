#pragma once

#include <string>
#include <grpcpp/grpcpp.h>

#include "proto/raftpb.grpc.pb.h"

class RaftNode;
class RaftCore;

class RaftServiceImpl final : public raftpb::raftService::Service {
public:
    ~RaftServiceImpl() {

    };

    RaftNode* rn;
    RaftCore* rc;

    grpc::Status RequestVote(grpc::ServerContext* context,const raftpb::ReqVote* req,raftpb::RespVote* reply) override;
    grpc::Status AppendEntries(grpc::ServerContext* context,const raftpb::ReqAppendEntry* req,raftpb::RespAppendEntry* reply) override;
    grpc::Status Tran(grpc::ServerContext* context,const raftpb::ReqTran* req,raftpb::RespTran* reply) override;

};

class RaftClient {
public:
    RaftClient(std::shared_ptr<grpc::Channel> channel) : stub_(raftpb::raftService::NewStub(channel)) {};
    RaftClient():stub_(nullptr){};
    std::unique_ptr<raftpb::raftService::Stub> stub_;

    uint64_t next_index;
};

void startRaftService(std::string address,RaftNode*,RaftCore*);
