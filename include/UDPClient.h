#ifndef __UDPClient_H__
#define __UDPClient_H__

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "SocketException.hpp"

class Socket {
 public:
  Socket();
  ~Socket();

  int Send(const void *buffer, size_t size);
  int SendTo(const char *buffer, size_t size, const char *host, uint16_t port);

  int Recv(void *buffer, size_t size);
  int RecvFrom(void *buffer, size_t size, char *host = nullptr, uint16_t *port = nullptr);

  int GetDescriptor();
  sockaddr_in GetDestinationAddress();
  sockaddr_in GetMyAddress();

 protected:
  int _sock_desc;
  sockaddr_in remoteAddrInfo;
  sockaddr_in _my_addr;
};

class UDPSocket : public Socket {};

class UDPClient : public UDPSocket {
 public:
  UDPClient() {}
  UDPClient(std::string ip, int port);
  ~UDPClient();
};

#endif