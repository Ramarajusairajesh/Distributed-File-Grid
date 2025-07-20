syntax = "proto3";

message Heartbeat {
  string server_id = 1;
  int64 timestamp = 2;
  double storage_used = 3;
  double storage_total = 4;
  double cpu_usage = 5;
  double network_bandwidth = 6;
}

message ChunkMetadata {
  string chunk_id = 1;
  int64 size = 2;
  string hash = 3;
  repeated string replica_servers = 4;
}

message FileChunk {
  string chunk_id = 1;
  bytes data = 2;
} 