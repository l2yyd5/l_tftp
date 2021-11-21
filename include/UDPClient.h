#ifndef __UDPClient_H__
#define __UDPClient_H__

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <string>

class Socket {
 public:
  Socket();
  ~Socket();

  int SendTo(const char *buffer, size_t size, const char *host, uint16_t port);

  int RecvFrom(void *buffer, size_t size, char *host = nullptr, uint16_t *port = nullptr);

  int GetDescriptor();
  sockaddr_in GetDestinationAddress();

 protected:
  int _sock_desc;
  sockaddr_in remoteAddrInfo;
};

class UDPSocket : public Socket {};

class UDPClient : public UDPSocket {
 public:
  UDPClient() {}
  UDPClient(std::string ip, int port);
  ~UDPClient();
};

#endif