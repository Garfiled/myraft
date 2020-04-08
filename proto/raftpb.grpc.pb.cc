// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: raftpb.proto

#include "raftpb.pb.h"
#include "raftpb.grpc.pb.h"

#include <functional>
#include <grpcpp/impl/codegen/async_stream.h>
#include <grpcpp/impl/codegen/async_unary_call.h>
#include <grpcpp/impl/codegen/channel_interface.h>
#include <grpcpp/impl/codegen/client_unary_call.h>
#include <grpcpp/impl/codegen/client_callback.h>
#include <grpcpp/impl/codegen/message_allocator.h>
#include <grpcpp/impl/codegen/method_handler.h>
#include <grpcpp/impl/codegen/rpc_service_method.h>
#include <grpcpp/impl/codegen/server_callback.h>
#include <grpcpp/impl/codegen/server_callback_handlers.h>
#include <grpcpp/impl/codegen/server_context.h>
#include <grpcpp/impl/codegen/service_type.h>
#include <grpcpp/impl/codegen/sync_stream.h>
namespace raftpb {

static const char* raftService_method_names[] = {
  "/raftpb.raftService/RequestVote",
  "/raftpb.raftService/AppendEntries",
  "/raftpb.raftService/Tran",
};

std::unique_ptr< raftService::Stub> raftService::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< raftService::Stub> stub(new raftService::Stub(channel));
  return stub;
}

raftService::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel)
  : channel_(channel), rpcmethod_RequestVote_(raftService_method_names[0], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_AppendEntries_(raftService_method_names[1], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_Tran_(raftService_method_names[2], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status raftService::Stub::RequestVote(::grpc::ClientContext* context, const ::raftpb::ReqVote& request, ::raftpb::RespVote* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_RequestVote_, context, request, response);
}

void raftService::Stub::experimental_async::RequestVote(::grpc::ClientContext* context, const ::raftpb::ReqVote* request, ::raftpb::RespVote* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_RequestVote_, context, request, response, std::move(f));
}

void raftService::Stub::experimental_async::RequestVote(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::raftpb::RespVote* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_RequestVote_, context, request, response, std::move(f));
}

void raftService::Stub::experimental_async::RequestVote(::grpc::ClientContext* context, const ::raftpb::ReqVote* request, ::raftpb::RespVote* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_RequestVote_, context, request, response, reactor);
}

void raftService::Stub::experimental_async::RequestVote(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::raftpb::RespVote* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_RequestVote_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::raftpb::RespVote>* raftService::Stub::AsyncRequestVoteRaw(::grpc::ClientContext* context, const ::raftpb::ReqVote& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::raftpb::RespVote>::Create(channel_.get(), cq, rpcmethod_RequestVote_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::raftpb::RespVote>* raftService::Stub::PrepareAsyncRequestVoteRaw(::grpc::ClientContext* context, const ::raftpb::ReqVote& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::raftpb::RespVote>::Create(channel_.get(), cq, rpcmethod_RequestVote_, context, request, false);
}

::grpc::Status raftService::Stub::AppendEntries(::grpc::ClientContext* context, const ::raftpb::ReqAppendEntry& request, ::raftpb::RespAppendEntry* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_AppendEntries_, context, request, response);
}

void raftService::Stub::experimental_async::AppendEntries(::grpc::ClientContext* context, const ::raftpb::ReqAppendEntry* request, ::raftpb::RespAppendEntry* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_AppendEntries_, context, request, response, std::move(f));
}

void raftService::Stub::experimental_async::AppendEntries(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::raftpb::RespAppendEntry* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_AppendEntries_, context, request, response, std::move(f));
}

void raftService::Stub::experimental_async::AppendEntries(::grpc::ClientContext* context, const ::raftpb::ReqAppendEntry* request, ::raftpb::RespAppendEntry* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_AppendEntries_, context, request, response, reactor);
}

void raftService::Stub::experimental_async::AppendEntries(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::raftpb::RespAppendEntry* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_AppendEntries_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::raftpb::RespAppendEntry>* raftService::Stub::AsyncAppendEntriesRaw(::grpc::ClientContext* context, const ::raftpb::ReqAppendEntry& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::raftpb::RespAppendEntry>::Create(channel_.get(), cq, rpcmethod_AppendEntries_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::raftpb::RespAppendEntry>* raftService::Stub::PrepareAsyncAppendEntriesRaw(::grpc::ClientContext* context, const ::raftpb::ReqAppendEntry& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::raftpb::RespAppendEntry>::Create(channel_.get(), cq, rpcmethod_AppendEntries_, context, request, false);
}

::grpc::Status raftService::Stub::Tran(::grpc::ClientContext* context, const ::raftpb::ReqTran& request, ::raftpb::RespTran* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_Tran_, context, request, response);
}

void raftService::Stub::experimental_async::Tran(::grpc::ClientContext* context, const ::raftpb::ReqTran* request, ::raftpb::RespTran* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_Tran_, context, request, response, std::move(f));
}

void raftService::Stub::experimental_async::Tran(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::raftpb::RespTran* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_Tran_, context, request, response, std::move(f));
}

void raftService::Stub::experimental_async::Tran(::grpc::ClientContext* context, const ::raftpb::ReqTran* request, ::raftpb::RespTran* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_Tran_, context, request, response, reactor);
}

void raftService::Stub::experimental_async::Tran(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::raftpb::RespTran* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_Tran_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::raftpb::RespTran>* raftService::Stub::AsyncTranRaw(::grpc::ClientContext* context, const ::raftpb::ReqTran& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::raftpb::RespTran>::Create(channel_.get(), cq, rpcmethod_Tran_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::raftpb::RespTran>* raftService::Stub::PrepareAsyncTranRaw(::grpc::ClientContext* context, const ::raftpb::ReqTran& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::raftpb::RespTran>::Create(channel_.get(), cq, rpcmethod_Tran_, context, request, false);
}

raftService::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      raftService_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< raftService::Service, ::raftpb::ReqVote, ::raftpb::RespVote>(
          [](raftService::Service* service,
             ::grpc_impl::ServerContext* ctx,
             const ::raftpb::ReqVote* req,
             ::raftpb::RespVote* resp) {
               return service->RequestVote(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      raftService_method_names[1],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< raftService::Service, ::raftpb::ReqAppendEntry, ::raftpb::RespAppendEntry>(
          [](raftService::Service* service,
             ::grpc_impl::ServerContext* ctx,
             const ::raftpb::ReqAppendEntry* req,
             ::raftpb::RespAppendEntry* resp) {
               return service->AppendEntries(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      raftService_method_names[2],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< raftService::Service, ::raftpb::ReqTran, ::raftpb::RespTran>(
          [](raftService::Service* service,
             ::grpc_impl::ServerContext* ctx,
             const ::raftpb::ReqTran* req,
             ::raftpb::RespTran* resp) {
               return service->Tran(ctx, req, resp);
             }, this)));
}

raftService::Service::~Service() {
}

::grpc::Status raftService::Service::RequestVote(::grpc::ServerContext* context, const ::raftpb::ReqVote* request, ::raftpb::RespVote* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status raftService::Service::AppendEntries(::grpc::ServerContext* context, const ::raftpb::ReqAppendEntry* request, ::raftpb::RespAppendEntry* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status raftService::Service::Tran(::grpc::ServerContext* context, const ::raftpb::ReqTran* request, ::raftpb::RespTran* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace raftpb
