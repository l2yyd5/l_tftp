#include <UDPClient.h>

Socket::Socket() {
  _sock_desc = -1;
}

Socket::~Socket() {
  close(_sock_desc);
}

int Socket::Send(const void *buffer, size_t size) {
  int status = sendto(_sock_desc, buffer, size, 0, (sockaddr *)&remoteAddrInfo, sizeof(remoteAddrInfo));
  // if (status == -1) throw SocketException("[Socket::Send()] sendto() failed");
  return status;
}

int Socket::SendTo(const char *buffer, size_t size, const char *host, uint16_t port) {
  struct sockaddr_in remoteAddr;
  remoteAddr.sin_family = AF_INET;
  remoteAddr.sin_addr.s_addr = host != nullptr ? ::inet_addr(host) : INADDR_ANY;
  remoteAddr.sin_port = htons(port);
  ::memset(remoteAddr.sin_zero, '\0', sizeof(remoteAddr.sin_zero));
  int status = sendto(_sock_desc, buffer, size, 0, (sockaddr *)&remoteAddr, sizeof(remoteAddr));
  // if (status == -1) throw SocketException("[Socket::Send()] sendto() failed");
  return status;
}

int Socket::Recv(void *buffer, size_t size) {
  int addr_len = sizeof(sockaddr_in);
  ::memset(&remoteAddrInfo, '\0', sizeof(remoteAddrInfo));
  int received = recvfrom(_sock_desc, buffer, size, 0, (sockaddr *)&remoteAddrInfo, (socklen_t *)&addr_len);
  bool recv_failed = received == -1;
  bool no_messages = received == 0;
  // if (recv_failed || no_messages) throw SocketException("[Socket::Recv()] recvfrom() failed");
  return received;
}

int Socket::RecvFrom(void *buffer, size_t size, char *host, uint16_t *port) {
  int addr_len = sizeof(sockaddr_in);
  ::memset(&remoteAddrInfo, '\0', sizeof(remoteAddrInfo));
  int received = recvfrom(_sock_desc, buffer, size, 0, (sockaddr *)&remoteAddrInfo, (socklen_t *)&addr_len);
  bool recv_failed = received == -1;
  bool no_messages = received == 0;
  // if (recv_failed || no_messages) throw SocketException("[Socket::Recv()] recvfrom() failed");
  if (received > 0) {
    if (host != nullptr) {
      ::strcpy(host, inet_ntoa(remoteAddrInfo.sin_addr));
    }
    if (port != nullptr) {
      *port = ntohs(remoteAddrInfo.sin_port);
    }
  }
  return received;
}

int Socket::GetDescriptor() {
  return _sock_desc;
}

sockaddr_in Socket::GetDestinationAddress() {
  return remoteAddrInfo;
}

sockaddr_in Socket::GetMyAddress() {
  return _my_addr;
}

UDPClient::UDPClient(std::string ip, int port) {
  _sock_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  bool socket_fail = _sock_desc == -1;

  remoteAddrInfo.sin_family = AF_INET;
  remoteAddrInfo.sin_port = htons(port);
  remoteAddrInfo.sin_addr.s_addr = inet_addr(ip.c_str());

  struct timeval timeout;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  ::setsockopt(_sock_desc, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
  timeout.tv_sec = 2;
  ::setsockopt(_sock_desc, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
}

UDPClient::~UDPClient() { close(_sock_desc); }