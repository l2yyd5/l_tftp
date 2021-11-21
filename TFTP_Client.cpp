#include <TFTPClient.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace std;

extern int optind, opterr, optopt;
extern char *optargi;

static struct option long_options[] = {
    {"help", no_argument, nullptr, 'h'},
    {"port", required_argument, nullptr, 'p'},
    {"addr", required_argument, nullptr, 'a'}};

const string WHITE_SPACE = " \t\r\n";

#define MAX_ARGV_LEN 128
#define SHOW_PANIC true
#define SHOW_WAIT_PANIC false

#define CHAR_BUF_SIZE 1024
char char_buf[CHAR_BUF_SIZE];

uint16_t port = 69;
string remoteaddr = "";
string mode = "octet";
string home_dir;

vector<string> cmd_history;
const string helpinfo(
    "\tUsage:\n"
    "\t\tls \n"
    "\t\tcd [~|dir] \n"
    "\t\tmode [mode(default octet)] \n"
    "\t\tput filename\n"
    "\t\tget filename\n"
    "\t\thistory \n"
    "\t\tclear \n"
    "\t\thelp \n"
    "\t\t? \n"
    "\t\tquit \n");

void panic(string hint, bool exit_ = false, int exit_code = 0) {
  if (SHOW_PANIC) cerr << "[!ExpShell panic]: " << hint << endl;
  if (exit_) exit(exit_code);
}

bool is_white_space(char ch) { return WHITE_SPACE.find(ch) != -1; }

vector<string> string_split(const string &s, const string &delims) {
  vector<string> vec;
  int p = 0, q;
  while ((q = s.find_first_of(delims, p)) != string::npos) {
    if (q > p) vec.push_back(s.substr(p, q - p));
    p = q + 1;
  }
  if (p < s.length()) vec.push_back(s.substr(p));
  return vec;
}

string string_split_last(const string &s, const string &delims) {
  vector<string> split_res = string_split(s, delims);
  return split_res.at(split_res.size() - 1);
}

string string_split_first(const string &s, const string &delims) {
  vector<string> split_res = string_split(s, delims);
  return split_res.at(0);
}

string trim(const string &s) {
  if (s.length() == 0) return string(s);
  int p = 0, q = s.length() - 1;
  while (is_white_space(s[p])) p++;
  while (is_white_space(s[q])) q--;
  return s.substr(p, q - p + 1);
}

string read_line() {
  string line;
  getline(cin, line);
  return line;
}

void show_command_prompt() {
  passwd *pwd = getpwuid(getuid());
  string username(pwd->pw_name);
  getcwd(char_buf, CHAR_BUF_SIZE);
  string cwd(char_buf);
  if (username == "root")
    home_dir = "/root";
  else
    home_dir = "/home/" + username;
  if (cwd == home_dir)
    cwd = "~";
  else if (cwd != "/") {
    cwd = string_split_last(cwd, "/");
  }
  gethostname(char_buf, CHAR_BUF_SIZE);
  string hostname(char_buf);
  hostname = string_split_first(hostname, ".");
  cout << "[" << username << "@" << hostname << " " << cwd
       << " tftp_client ]> ";
}

void check_wait_status(int &wait_status) {
  if (WIFEXITED(wait_status) == 0) {
    char buf[8];
    sprintf(buf, "%d", WEXITSTATUS(wait_status));
    if (SHOW_WAIT_PANIC) panic("child exit with code " + string(buf));
  }
}

int process_builtin_command(string line) {
  if (line == "cd") {
    chdir(home_dir.c_str());
    return 1;
  } else if (line.substr(0, 2) == "cd") {
    string arg1 = string_split(line, WHITE_SPACE)[1];
    if (arg1.find("~") == 0) line = "cd " + home_dir + arg1.substr(1);
    int chdir_ret = chdir(trim(line.substr(2)).c_str());
    if (chdir_ret < 0) {
      panic("chdir failed");
      return -1;
    } else
      return 1;
  }

  if (line == "ls") {
    system("ls");
    return 1;
  }

  if (line == "clear") {
    system("clear");
    return 1;
  }

  if (line == "history") {
    for (int i = cmd_history.size() - 1; i >= 0; i--)
      cout << "\t" << i << "\t" << cmd_history.at(i) << endl;
    return 1;
  }

  if (line == "?") {
    cout << helpinfo << endl;
    return 1;
  }

  if (line == "help") {
    cout << helpinfo << endl;
    return 1;
  }

  if (line == "quit") {
    cout << "Bye~~~!" << endl;
    exit(0);
  }

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    cout << "Missing remote address\n" << endl;
    exit(0);
  }
  system("clear");
  int index = 0;
  int c = 0;
  while (EOF != (c = getopt_long(argc, argv, "hvn:", long_options, &index))) {
    switch (c) {
      case 'h':
        cout << "Usage:   tftp [--addr | -a] addr [--port | -p] port\n  \n";
        break;
      case 'a':
        remoteaddr = optarg;
        break;
      case 'p':
        port = stoi(optarg);
        break;
      case '?':
        cout << "unknow option: " << optopt << "\n\n"
             << "Usage:   tftp [--addr | -a] addr [--port | -p] port\n  \n";
        exit(0);
        break;
      default:
        break;
    }
  }
  if (remoteaddr.length() == 0) {
    cout << "Invalid parameters!\n\n"
         << "Usage:   tftp [--addr | -a] addr [--port | -p] port\n  \n";
    exit(0);
  }

  TFTPClient remote(remoteaddr, port, mode);
  string line;
  int wait_status;
  while (true) {
    show_command_prompt();
    line = trim(read_line());
    cmd_history.push_back(line);
    if (process_builtin_command(line) > 0) continue;

    vector<string> args = string_split(line, WHITE_SPACE);
    transform(args[0].begin(), args[0].end(), args[0].begin(),
              [](unsigned char c) { return tolower(c); });
    TFTPClient::status st;
    if (args.size() == 0)
      continue;
    else if (args.size() == 1) {
      cout << "command not found: " << args[0] << endl;
      continue;
    } else {
      if (args[0] == "get") {
        st = remote.get(args[1]);
        if (st != TFTPClient::status::Success) {
          cout << remote.errorDescription(st) << endl;
          remote.writeLog(remote.errorDescription(st));
          continue;
        }
      } else if (args[0] == "put") {
        st = remote.put(args[1]);
        if (st != TFTPClient::status::Success) {
          cout << remote.errorDescription(st) << endl;
          remote.writeLog(remote.errorDescription(st));
        }
        continue;
      } else if (args[0] == "mode") {
        cout << "Currently in " << mode << " mode." << endl;
        transform(args[1].begin(), args[1].end(), args[1].begin(),
                  [](unsigned char c) { return tolower(c); });
        mode = args[1];
        remote.changeMode(mode);
        cout << "Change the mode to " << mode << "." << endl;
        continue;
      } else {
        cout << helpinfo << endl;
        continue;
      }
    }
    wait(&wait_status);
    check_wait_status(wait_status);
  }
  return 0;
}
