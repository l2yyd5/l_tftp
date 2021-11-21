#ifndef __SOCKETCLIENT_MYEXCEPTION_H__
#define __SOCKETCLIENT_MYEXCEPTION_H__
#include <cstring>
#include <exception>
#include <string>

struct SocketException : public std::exception {
  std::string s;
  SocketException(std::string ss) : s(ss) {
    s.append("\n");
    s += std::string(strerror(errno));
  }
  ~SocketException() throw() {}
  const char* what() const throw() { return s.c_str(); }
};

#endif