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
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>

#include "shared/netraw.h"

using std::string;

static const int kMaxDatagramSize = 65536;
bool run_ = true;

// Class to asynchronously list to messges from SSL-Vision, and synchronously
// log them to a combined log file.
class SSLVisionLogger {
 public:
  SSLVisionLogger() {}

  // Initializes the logger, returns true iff succesful.
  bool Initialize() {
    return true;
  }

 private:
  static void* SSLVisionLoggerThread(void* vision_logger) {}
};

// Class to asynchronously list to messges from referees (human, or automatic),
// and synchronously log them to a combined log file.
class SSLRefereeLogger {
 public:
  explicit SSLRefereeLogger(int port_number) : port_number_(port_number) {
    pthread_create(&logger_thread_,
                   NULL,
                   SSLRefereeLogger::SSLRefereeLoggerThread,
                   reinterpret_cast<void*>(this));
  }

 private:
  static void* SSLRefereeLoggerThread(void* referee_logger_ptr) {
    SSLRefereeLogger &referee_logger =
        *(reinterpret_cast<SSLRefereeLogger*>(referee_logger_ptr));
    // Initialize network multicast client.
    const string net_address("224.5.23.1");
    Net::UDP client;
    Net::Address multiaddr,interface;
    multiaddr.setHost(net_address.c_str(), referee_logger.port_number_);
    interface.setAny();
    if(!client.addMulticast(multiaddr,interface)) {
      fprintf(stderr,
              "Unable to setup UDP multicast for referee port %d\n",
              referee_logger.port_number_);
      fflush(stderr);
      return NULL;
    }

    // Start receive loop.
    Net::Address src;
    char* buffer = new char[kMaxDatagramSize];
    while (run_) {
      const int bytes_received = client.recv(buffer, kMaxDatagramSize, src);
      if (bytes_received>0) {
        // Log data.
      }
    }
    delete buffer;
    return NULL;
  }

  pthread_t logger_thread_;
  const int port_number_;
};

int main(int argc, char *argv[]) {
  if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "-?") == 0)) {
    printf("Usage: logger [port for autoref1] [port for autoref2] ...\n");
    return 0;
  }

  // Parse arguments to get the list of autoref ports.
  const int num_autorefs = argc - 1;

  // Initialize SSL-Vision, refbox clients

  // Initialize autoref clients

  return 0;
}
