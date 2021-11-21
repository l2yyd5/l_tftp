#ifndef __TFTPClient_H__
#define __TFTPClient_H__

#include <UDPClient.h>

#include <array>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include <sstream>

using boost::format;
using namespace boost::posix_time;

class TFTPClient : public UDPClient {
  enum OperationCode {
    RRQ = 1,
    WRQ,
    DATA,
    ACK,
    ERR,
    OACK
  };

 public:
  enum status {
    Success = 0,
    InvalidSocket,
    WriteError,
    ReadError,
    UnexpectedPacketReceived,
    EmptyFilename,
    OpenFileError,
    WriteFileError,
    ReadFileError,
    TimeOut
  };

  TFTPClient() {}
  TFTPClient(std::string ip, int port, std::string mode);
  ~TFTPClient() {
    logfile.flush();
    logfile.close();
  }

  void changeMode(const std::string &mode);
  void writeLog(const std::string Message);
  status get(const std::string &fileName);
  status put(const std::string &fileName);
  std::string errorDescription(status code);

 private:
  static constexpr uint8_t m_headerSize = 4;
  static constexpr uint16_t m_dataSize = 512;

  using Result = std::pair<status, int32_t>;
  using Buffer = std::array<char, m_headerSize + m_dataSize>;

  Result sendRequest(const std::string &fileName, OperationCode code);
  Result sendAck();
  Result read();
  Result getFile(std::fstream &file, const ptime &startime, double &loseper);
  Result putFile(std::fstream &file, const ptime &startime, double &loseper);

  std::fstream logfile;
  UDPClient m_socket;
  std::string m_remoteAddress;
  std::string m_mode;
  uint16_t m_port;
  uint16_t m_remotePort;
  Buffer m_buffer;
  uint16_t m_receivedBlock;
};

#endif