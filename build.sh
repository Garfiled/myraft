g++ -ggdb myraft.cc  wal.cc timer.cc raftserviceimpl.cc proto/raftpb.grpc.pb.cc proto/raftpb.pb.cc -std=c++11 -o myraft -lprotobuf -lgrpc++