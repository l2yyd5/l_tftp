#include <TFTPClient.h>

TFTPClient::TFTPClient(std::string ip, int port, std::string mode)
    : m_socket(ip, port),
      m_remoteAddress(ip),
      m_port(port),
      m_remotePort(0),
      m_receivedBlock(0),
      m_mode(mode) {
  m_status = status::Success;

  ptime nowtime = second_clock::local_time();
  std::string logPath = to_simple_string(nowtime);
  logPath[11] = '-';
  logPath = "logs/" + logPath + ".log";
  logfile.open(logPath.c_str(),
               std::fstream::in | std::fstream::out | std::fstream::app);
}

void TFTPClient::changeMode(const std::string &mode) { m_mode = mode; }

void TFTPClient::writeLog(const std::string Message) {
  std::string messages = to_simple_string(second_clock::local_time());
  messages[11] = '-';
  messages += ":\t" + Message;
  messages += "\n";
  logfile.write(messages.c_str(), messages.length());
  logfile.flush();
}

std::string TFTPClient::errorDescription(TFTPClient::status code) {
  switch (code) {
    case status::Success:
      return "Success.\n";
    case InvalidSocket:
      return "Error! Invalid Socket.\n";
    case WriteError:
      return "Error! Write Socket.\n";
    case ReadError:
      return "Error! Read Socket.\n";
    case UnexpectedPacketReceived:
      return "Error! Unexpected Packet Received.\n";
    case EmptyFilename:
      return "Error! Empty Filename.\n";
    case OpenFileError:
      return "Error! Can't Open File.\n";
    case WriteFileError:
      return "Error! Write File.\n";
    case ReadFileError:
      return "Error! Read File.\n";
    case TimeOut:
      return "Error! Server Timeout.\n";
    default:
      return "Error!\n";
  }
}

TFTPClient::Result TFTPClient::sendRequest(const std::string &fileName,
                                           OperationCode code) {
  if (fileName.empty()) {
    return std::make_pair(status::EmptyFilename, 0);
  }

  m_buffer[0] = 0;
  m_buffer[1] = static_cast<char>(code);

  char *end = std::strncpy(&m_buffer[2], fileName.c_str(), fileName.size()) +
              fileName.size();
  *end++ = '\0';

  end = std::strncpy(end, m_mode.c_str(), m_mode.size()) + m_mode.size();
  *end++ = '\0';

  const auto packetSize = std::distance(&m_buffer[0], end);
  const auto sendNums = m_socket.SendTo(&m_buffer[0], packetSize,
                                        m_remoteAddress.c_str(), m_port);
  if (sendNums != packetSize) {
    return std::make_pair(status::WriteError, sendNums);
  }
  return std::make_pair(status::Success, sendNums);
}

TFTPClient::Result TFTPClient::sendAck() {
  const size_t packetSize = 4;
  m_buffer[0] = 0;
  m_buffer[1] = static_cast<char>(OperationCode::ACK);

  const auto sendNums = m_socket.SendTo(&m_buffer[0], packetSize,
                                        m_remoteAddress.c_str(), m_remotePort);
  const bool isSuccess = sendNums == packetSize;
  return std::make_pair(isSuccess ? status::Success : status::WriteError,
                        sendNums);
}

TFTPClient::Result TFTPClient::read() {
  std::string errorMessage;
  const auto recvNums = m_socket.RecvFrom(&m_buffer[0], m_buffer.size(),
                                          &m_remoteAddress[0], &m_remotePort);
  if (recvNums == -1) {
    this->writeLog("Error! Timeout!\n");
    std::puts("Error! Timeout!\n");
    return std::make_pair(status::TimeOut, recvNums);
  }
  const auto code = static_cast<OperationCode>(m_buffer[1]);
  switch (code) {
    case OperationCode::DATA:
      m_receivedBlock = ((uint8_t)m_buffer[2] << 8) | (uint8_t)m_buffer[3];
      return std::make_pair(status::Success, recvNums - m_headerSize);
    case OperationCode::ACK:
      m_receivedBlock = ((uint8_t)m_buffer[2] << 8) | (uint8_t)m_buffer[3];
      return std::make_pair(status::Success, m_receivedBlock);
    case OperationCode::ERR:
      errorMessage =
          (format("\nError! Message from remote host: %s.\n") % &m_buffer[4])
              .str();
      this->writeLog(errorMessage);
      std::cout << errorMessage << std::endl;
      return std::make_pair(status::ReadError, recvNums);
    default:
      errorMessage =
          (format("\nError! Unexpected packet received! Type: %i.\n") % code)
              .str();
      this->writeLog(errorMessage);
      std::cout << errorMessage << std::endl;
      return std::make_pair(status::UnexpectedPacketReceived, recvNums);
  }
}

TFTPClient::Result TFTPClient::getFile(std::fstream &file,
                                       const ptime &startime, double &loseper) {
  uint16_t totalRecvBlocks = 0;
  uint16_t recvNums = 0;
  unsigned int totalRecvNums = 0;
  Result result;
  int losetimes = 0;
  int recvt = 0, loset = 0;
  while (true) {
    result = this->read();
    recvt++;
    if (result.first != status::Success) {
      losetimes++;
      loset++;
      if (losetimes > 3)
        return std::make_pair(result.first,
                              totalRecvNums + std::max(result.second, 0));
      continue;
    }
    losetimes = 0;
    recvNums = result.second;
    if ((recvNums > 0) && (m_receivedBlock > totalRecvBlocks) ||
        totalRecvBlocks == 65535) {
      ++totalRecvBlocks;
      totalRecvNums += recvNums;
      file.write(&m_buffer[m_headerSize], recvNums);
      if (file.bad()) {
        return std::make_pair(status::WriteFileError, totalRecvNums);
      }
      result = this->sendAck();
      recvt++;
    } else if (m_receivedBlock == totalRecvBlocks) {
      loset++;
      recvt++;
      result = this->sendAck();
      if (result.first != status::Success) {
        losetimes = 1;
        while (losetimes++ < 3) {
          result = this->sendAck();
          if (result.first == status::Success) {
            break;
          }
        }
        recvt += losetimes;
        loset += losetimes;
        if (losetimes == 3) {
          return std::make_pair(result.first, totalRecvNums);
        }
      }
    }

    ptime now = microsec_clock::local_time();
    time_duration dur = now - startime;
    long long nanoseconds = dur.total_nanoseconds();
    double kbs = 1000000.0 * totalRecvNums;
    kbs /= (nanoseconds * 1.0);
    printf("%u bytes (%i blocks) received. speed %.2lf kb/s.\n", totalRecvNums,
           totalRecvBlocks, kbs);

    if (recvNums != m_dataSize) break;
  }
  loseper = (loset * 100.0) / (recvt * 1.0);
  return std::make_pair(status::Success, totalRecvNums);
}

TFTPClient::Result TFTPClient::putFile(std::fstream &file,
                                       const ptime &starttime,
                                       double &loseper) {
  Result result;
  uint16_t currentBlock = 0;
  unsigned int totalSendNums = 0;
  Buffer backs;
  int losetimes = 0, loses = 0;
  int recvt = 0, loset = 0;
  while (true) {
    if (currentBlock == m_receivedBlock) {
      if (file.eof()) {
        loseper = (100.0 * loset) / (1.0 * recvt);
        return std::make_pair(status::Success, totalSendNums);
      }
      loses = 0;
      ++currentBlock;
      m_buffer[0] = 0;
      m_buffer[1] = static_cast<char>(OperationCode::DATA);
      m_buffer[2] = static_cast<uint8_t>(currentBlock >> 8);
      m_buffer[3] = static_cast<uint8_t>(currentBlock & 0xff);
      file.read(&m_buffer[m_headerSize], m_dataSize);
      if (file.bad()) {
        return std::make_pair(status::ReadFileError, totalSendNums);
      }
      memcpy(&backs[0], &m_buffer[0], m_headerSize + m_dataSize);
    } else {
      loses++;
      loset++;
      if (loses == 3) {
        return std::make_pair(result.first, totalSendNums);
      }
      memcpy(&m_buffer[0], &backs[0], m_headerSize + m_dataSize);
    }
    const auto packetSize = m_headerSize + file.gcount();
    const auto sendNums = m_socket.SendTo(
        &m_buffer[0], packetSize, m_remoteAddress.c_str(), m_remotePort);
    recvt++;
    if (sendNums != packetSize) {
      return std::make_pair(status::WriteError, totalSendNums + sendNums);
    }

    result = this->read();
    recvt++;
    if (result.first != status::Success || m_receivedBlock != currentBlock) {
      result = this->read();
      if (result.first != status::Success) {
        losetimes = 1;
        while (losetimes++ < 3) {
          result = this->read();
          if (result.first == status::Success) {
            break;
          }
        }
        recvt += losetimes;
        loset += losetimes;
      }
    }

    totalSendNums += file.gcount();

    ptime now = microsec_clock::local_time();
    time_duration dur = now - starttime;
    long long nanoseconds = dur.total_nanoseconds();
    double kbs = 1000000.0 * totalSendNums;
    kbs /= (nanoseconds * 1.0);
    printf("%u bytes (%i blocks) written. Speed %.2lf kb/s.\n", totalSendNums,
           currentBlock, kbs);
  }
  loseper = (100.0 * loset) / (1.0 * recvt);
  return std::make_pair(status::Success, totalSendNums);
}

TFTPClient::status TFTPClient::get(const std::string &fileName) {
  if (m_status != status::Success) {
    return m_status;
  }
  double loset = 0;
  ptime startTime = microsec_clock::local_time();
  std::fstream file;
  if (m_mode == "netascii") {
    file.open(fileName.c_str(), std::ios_base::out);
  } else if (m_mode == "octet") {
    file.open(fileName.c_str(), std::ios_base::out | std::ios_base::binary);
  }
  if (!file.is_open()) {
    this->writeLog("Error! Can't open file!\n");
    std::puts("Error! Can't open file!\n");
    file.close();
    return status::OpenFileError;
  }

  Result result = this->sendRequest(fileName, OperationCode::RRQ);
  if (result.first != status::Success) {
    file.close();
    return result.first;
  }

  m_receivedBlock = 0;
  result = this->getFile(file, startTime, loset);
  if (result.first == status::Success) {
    ptime now = microsec_clock::local_time();
    time_duration dur = now - startTime;
    long long nanoseconds = dur.total_nanoseconds();
    double kbs = 1000000.0 * result.second;
    kbs /= (nanoseconds * 1.0);
    std::string successMessage =
        (format("\n%d bytes received! Speed %.2lf kb/s.\nGet file "
                "successfully! (%%%.1lf lose)\n") %
         result.second % kbs % loset)
            .str();
    this->writeLog(successMessage);
    std::cout << successMessage;
  }
  file.close();
  return result.first;
}

TFTPClient::status TFTPClient::put(const std::string &fileName) {
  if (m_status != status::Success) {
    return m_status;
  }

  double loset = 0;
  ptime startTime = microsec_clock::local_time();
  std::fstream file;
  if (m_mode == "netascii") {
    file.open(fileName.c_str(), std::ios_base::in);
  } else if (m_mode == "octet") {
    file.open(fileName.c_str(), std::ios_base::in | std::ios_base::binary);
  }
  if (!file) {
    this->writeLog("Error! Can't open file!\n");
    std::puts("Error! Can't open file!\n");
    file.close();
    return status::OpenFileError;
  }

  Result result = this->sendRequest(fileName, OperationCode::WRQ);
  if (result.first != status::Success) {
    file.close();
    return result.first;
  }

  m_receivedBlock = 0;
  result = this->read();
  if (result.first != status::Success) {
    file.close();
    return result.first;
  }

  result = this->putFile(file, startTime, loset);
  if (result.first == status::Success) {
    ptime now = microsec_clock::local_time();
    time_duration dur = now - startTime;
    long long nanoseconds = dur.total_nanoseconds();
    double kbs = 1000000.0 * result.second;
    kbs /= (nanoseconds * 1.0);
    std::string successMessage =
        (format("\n%d bytes sent! Speed %.2lf kb/s.\nPut file successfully! "
                "(%%%.1lf lose)\n") %
         result.second % kbs % loset)
            .str();
    this->writeLog(successMessage);
    std::cout << successMessage;
  }
  file.close();
  return result.first;
}