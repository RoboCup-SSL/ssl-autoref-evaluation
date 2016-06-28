//========================================================================
//  This software is free: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License Version 3,
//  as published by the Free Software Foundation.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  Version 3 in the file COPYING that came with this distribution.
//  If not, see <http://www.gnu.org/licenses/>.
//========================================================================
// Copyright 2016 joydeepb@cs.umass.edu
// College of Information and Computer Sciences
// University of Massachusetts Amherst
//
// Logger for SSL-Vision, refbox, and multiple automatic referees.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "shared/netraw.h"
#include "shared/misc_util.h"
#include "shared/pthread_utils.h"
#include "shared/util.h"
#include "udp_message_wrapper.pb.h"

using std::string;
using std::vector;

// Maximum size of UDP datagrams to receive.
static const int kMaxDatagramSize = 65536;

// UDP Multicast address for referees.
static const char* kRefereeMulticast = "224.5.23.1";

// UDP Multicast address for SSL Vision.
static const char* kVisionMulticast = "224.5.23.2";

// Port number for SSL Vision.
static const int kVisionPort = 10006;

// Port number for main refbox.
static const int kRefboxPort = 10003;

// Flag to handle graceful shutdown of multiple threads on SIGINT.
bool run_ = true;

// Mutex for writing to the log file.
pthread_mutex_t logging_mutex_ = PTHREAD_MUTEX_INITIALIZER;

// Handle to log file.
FILE* log_file_ = NULL;

// Verbose mode: print referee events as they are logged.
bool verbose = false;

// Class to asynchronously list to messges from protobuf encoded UDP packets,
// and synchronously log them to a combined log file.
class ProtobufLogger {
 public:
  // Disable default constructor, and copy constructor.
  ProtobufLogger();
  ProtobufLogger(const ProtobufLogger&);

  // Main constructor, that accepts a UDP address and port number to listen on.
  explicit ProtobufLogger(const std::string&ip_address, int port_number) :
      ip_address_(ip_address), port_number_(port_number) {
    printf("Logging from %s:%d\n", ip_address_.c_str(), port_number_);
    pthread_create(&logger_thread_,
                   NULL,
                   ProtobufLogger::LoggerThread,
                   reinterpret_cast<void*>(this));
  }

  // Main destructor, just waits for logger thread to terminate.
  // TODO(joydeepb): Eliminate potential deadlock by actually terminating the
  //   logger thread.
  ~ProtobufLogger() {
    // pthread_join(logger_thread_, NULL);
  }

  pthread_t GetThreadID() {
    return logger_thread_;
  }

 private:
  static void* LoggerThread(void* logger_ptr) {
    static const bool kDebug = false;
    ProtobufLogger &logger =
        *(reinterpret_cast<ProtobufLogger*>(logger_ptr));
    // Initialize network multicast client.
    const string net_address(logger.ip_address_.c_str());
    Net::UDP client;
    Net::Address multiaddr,interface;
    multiaddr.setHost(net_address.c_str(), logger.port_number_);
    interface.setAny();

    if(!client.open(logger.port_number_, true, true, true)) {
      fprintf(stderr,
              "Unable to open UDP network port %d\n",
              logger.port_number_);
      fflush(stderr);
      return NULL;
    }

    if(!client.addMulticast(multiaddr,interface)) {
      fprintf(stderr,
              "Unable to set up UDP multicast for %s:%d\n",
              logger.ip_address_.c_str(),
              logger.port_number_);
      fflush(stderr);
      perror("UDP Error");
      return NULL;
    }

    // Start receive loop.
    Net::Address src;
    char* receive_buffer = new char[kMaxDatagramSize];
    string write_buffer;
    UDPMessageWrapper message_wrapper;
    message_wrapper.set_address(logger.ip_address_);
    message_wrapper.set_port(logger.port_number_);
    while (run_) {
      const int bytes_received =
          client.recv(receive_buffer, kMaxDatagramSize, src);
      if (bytes_received>0) {
        if (verbose || kDebug) {
          printf("Received %d bytes from %s:%d\n",
                bytes_received,
                logger.ip_address_.c_str(),
                logger.port_number_);
        }
        message_wrapper.set_timestamp(GetTimeUSec());
        message_wrapper.set_data(receive_buffer, bytes_received);
        message_wrapper.SerializeToString(&write_buffer);
        // Log data.
        ScopedLock lock(logging_mutex_);

        // Write size of packet.
        const uint32_t packet_size = write_buffer.size();
        fwrite(&packet_size, sizeof(packet_size), 1, log_file_);

        // Write packet payload.
        fwrite(write_buffer.data(), 1, packet_size, log_file_);
      }
    }
    delete receive_buffer;
    return NULL;
  }

  pthread_t logger_thread_;
  const std::string ip_address_;
  const int port_number_;
};

void SigIntHandler(int) {
  run_ = false;
  printf("\nClosing.\n");
  fflush(stdout);
}

void PrintUsage() {
  printf("Usage: logger [-v] [address1:port1] [address2:port2] ...\n");
}

string GetFileName() {
  char date_string[256];
  char file_name[256];
  // Current time in timezone-independent format.
  time_t time_now = time(NULL);
  // Milliseconds elapsed since the last second.
  const int milliseconds = ((GetTimeUSec() / 1000) % 1000);
  // Current time in local timezone.
  tm* local_time = localtime(&time_now);
  // Convert time to formatted date-time string.
  strftime(date_string, sizeof(date_string), "%Y-%m-%d-%H-%M-%S", local_time);
  // Complete filename will be in the form "YYYY-MM-DD-SS-[ms].log"
  sprintf(file_name, "%s-%03d.log", date_string, milliseconds);
  return (string(file_name));
}

bool ParseAddressAndPort(const char* arg, string* address, int* port) {
  const int arg_len = strlen(arg);
  const char* split = strstr(arg, ":");
  if (arg_len == 0 || split == NULL || split + 1 == arg + arg_len) return false;
  *port = atoi(split + 1);
  address->assign(arg, split - arg);
  return true;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    PrintUsage();
    printf("No autorefs listed, logging only SSL-Vision and refbox.\n");
  } else if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "-?") == 0) {
    PrintUsage();
    return 0;
  }
  signal(SIGINT, SigIntHandler);

  // Initialize clients, log file.
  const string file_name = GetFileName();
  printf("Logging to %s\n", file_name.c_str());
  log_file_ = fopen(file_name.c_str(), "w");

  vector<ProtobufLogger*> loggers;
  // Create logger for SSL Vision.
  loggers.push_back(new ProtobufLogger(kVisionMulticast, kVisionPort));
  // Create logger for main refbox.
  loggers.push_back(new ProtobufLogger(kRefereeMulticast, kRefboxPort));

  // Create loggers for all specified additional referee sources.
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-v") == 0) {
      printf("Verbose mode\n");
      verbose = true;
      continue;
    }

    int port_number = 0;
    string address;
    if (ParseAddressAndPort(argv[i], &address, &port_number)) {
      loggers.push_back(new ProtobufLogger(address, port_number));
    }
    // const int port_number = atoi(argv[i]);
    // loggers.push_back(new ProtobufLogger(kRefereeMulticast, port_number));
    // printf("Logging autoref %s:%d\n", kRefereeMulticast, port_number);
  }

  // Initialize autoref clients
  static const int kSleepPeriod = 2000;
  while (run_) {
    usleep(kSleepPeriod);
  }

  // Close and quit.
  for (int i = 0; i < loggers.size(); ++i) {
    delete loggers[i];
    loggers[i] = NULL;
  }

  fclose(log_file_);
  return 0;
}
