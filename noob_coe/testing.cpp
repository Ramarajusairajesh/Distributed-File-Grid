#pragma once
#include "../protos/v1/generate/heart_beat.pb.h"
#include <arpa/inet.h>
#include <chrono>
#include <coroutine>
#include <cstring>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/util/time_util.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
int send_signal(std::string server_ip, int server_id, int port = 9000) {
  int csfd = socket(AF_INET, SOCK_STREAM, 0);
  char host[256];
  struct hostent *h;
  gethostname(host, sizeof(host));
  h = gethostbyname(host);
  if (!h) {
    std::cerr << "gethostname error \"Can't find hostname\"" << std::endl;
    return 1;
  }

  if (csfd < 0) {
    std::cerr << "Error creating a socket" << std::endl;
    return 1;
  }

  struct sockaddr_in serveraddr;
  memset(&serveraddr, 0, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(port);
  serveraddr.sin_addr.s_addr = INADDR_ANY;
  inet_pton(AF_INET, server_ip.c_str(), &serveraddr.sin_addr);

  std::string message = "Hello";
  heart_beat::v1::HeartBeat hb;
  hb.set_ip(inet_ntoa(*(struct in_addr *)h->h_addr));
  hb.set_server_id(server_id);
  std::string payload;
  if (hb.SerializeToString(&payload)) {
    std::cerr << "Failed to serialize heart beat signal" << std::endl;
    return 1;
  }
  uint32_t payload_size = htonl(static_cast<uint32_t>(payload.size()));

  if (connect(csfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
    std::cerr << "Error connecting to the server" << std::endl;
  }
  while (true) {
    google::protobuf::Timestamp *ts = hb.mutable_timestamp();
    *ts = google::protobuf::util::TimeUtil::GetCurrentTime();
    if (send(csfd, &payload_size, sizeof(payload_size), 0) <= 0) {
      std::cerr << "Error sending the heartbeat signal" << std::endl;
      return 1;
    }
    if (send(csfd, &payload, sizeof(payload), 0) <= 0) {
      std::cerr << "Error sending the heartbeat signal payload" << std::endl;
      return 1;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  close(csfd);
  return 0;
}

int recieve_signal() { return 0; }
