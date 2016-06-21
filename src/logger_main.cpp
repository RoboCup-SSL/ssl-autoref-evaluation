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
#include "shared/pthread_utils.h"

using std::string;
using std::vector;

// Maximum size of UDP datagrams to receive.
static const int kMaxDatagramSize = 65536;

// UDP Multicast address for referees.
static const char* kRefereeMulticast = "224.5.23.2";

// UDP Multicast address for SSL Vision.
static const char* kVisionMulticast = "224.5.23.2";

// Port number for SSL Vision.
static const int kVisionPort = 10006;

// Port number for main refbox.
static const int kRefboxPort = 10002;

// Flag to handle graceful shutdown of multiple threads on SIGINT.
bool run_ = true;

// Mutex for writing to the log file.
pthread_mutex_t logging_mutex_ = PTHREAD_MUTEX_INITIALIZER;

// Handle to log file.
FILE* log_file_ = NULL;

// Get timestamp as reported by gettimeofday, in number of microseconds
uint64_t GetTimeUSec();

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
    char* buffer = new char[kMaxDatagramSize];
    while (run_) {
      const int bytes_received = client.recv(buffer, kMaxDatagramSize, src);
      if (bytes_received>0) {
        const uint64_t timestamp = GetTimeUSec();
        // Log data.
        ScopedLock lock(logging_mutex_);
        // TODO(joydeepb): Handle byte-order in file I/O to be agnostic of
        // machine type.

        // Write port number, which implicitly also encodes type: Vision
        // messages are received on port 10006, refbox on 10002, autorefs on
        // any other port.
        fwrite(&(logger.port_number_),
               sizeof(logger.port_number_),
               1,
               log_file_);

        // Write timestamp.
        fwrite(&timestamp, sizeof(timestamp), 1, log_file_);

        // Write size of packet.
        fwrite(&bytes_received, sizeof(bytes_received), 1, log_file_);

        // Write packet payload.
        fwrite(buffer, 1, bytes_received, log_file_);
      }
    }
    delete buffer;
    return NULL;
  }

  pthread_t logger_thread_;
  const std::string ip_address_;
  const int port_number_;
};

uint64_t GetTimeUSec() {
  timespec time;
  clock_gettime(CLOCK_REALTIME, &time);
  const uint64_t seconds = time.tv_sec;
  const uint64_t useconds = time.tv_nsec / 1000;
  return (seconds * 1000000 + useconds);
}

void SigIntHandler(int) {
  run_ = false;
  printf("\nClosing.\n");
  fflush(stdout);
}

int main(int argc, char *argv[]) {
  if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "-?") == 0)) {
    printf("Usage: logger [port for autoref1] [port for autoref2] ...\n");
    return 0;
  }

  signal(SIGINT, SigIntHandler);

  // Initialize clients, log file.
  log_file_ = fopen("game_log.log", "w");
  ProtobufLogger vision_logger(kVisionMulticast, kVisionPort);

  const int num_autorefs = argc - 1;
  vector<ProtobufLogger*> loggers;
  // Create logger for main refbox.
  loggers.push_back(new ProtobufLogger(kRefereeMulticast, kRefboxPort));

  // Create loggers for all specified additional referee sources.
  for (int i = 0; i < num_autorefs; ++i) {
    const int port_number = atoi(argv[i + 1]);
    loggers.push_back(new ProtobufLogger(kRefereeMulticast, port_number));
    printf("Logging referee %s:%d\n", kRefereeMulticast, port_number);
  }

  // Initialize autoref clients
  static const int kSleepPeriod = 2000;
  while (run_) {
    usleep(kSleepPeriod);
  }

  // Close and quit.

  return 0;
}
