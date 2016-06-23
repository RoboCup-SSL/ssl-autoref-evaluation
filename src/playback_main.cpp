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
// Log playback for SSL-Vision, refbox, and multiple automatic referees.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include "shared/netraw.h"
#include "shared/util.h"
#include "udp_message_wrapper.pb.h"

using std::max;
using std::string;
using std::vector;


// UDP publisher.
Net::UDP publisher_;

bool ReadUDPMessageWrapper(FILE* fid, UDPMessageWrapper* message) {
  uint32_t packet_size = 0;
  if (fread(&packet_size, sizeof(packet_size), 1, fid) != 1) {
    return false;
  }
  char* read_buffer = new char[packet_size];
  const bool success = (fread(read_buffer, 1, packet_size, fid) == packet_size);
  if (success) {
    message->ParseFromArray(read_buffer, packet_size);
  } else {
    fprintf(stderr, "Error reading packet data of size %d", packet_size);
    perror("");
  }
  delete[] read_buffer;
  return success;
}

void PublishMessage(const UDPMessageWrapper& message) {
  Net::Address address;
  address.setHost(message.address().c_str(), message.port());
  const string& message_data = message.data();
  if (!publisher_.send(message_data.data(),
      message_data.size(),
      address)) {
    perror("Sendto Error");
    fprintf(stderr,
            "Sending UDP datagram to %s:%d failed (maybe too large?). "
            "Size was: %zu byte(s)\n",
            message.address().c_str(),
            message.port(),
            message_data.size());
  }
}

uint64_t GetTimeUSec() {
  timespec time;
  clock_gettime(CLOCK_REALTIME, &time);
  const uint64_t seconds = time.tv_sec;
  const uint64_t useconds = time.tv_nsec / 1000;
  return (seconds * 1000000 + useconds);
}

void PlayLogFile(const string& log_file) {
  static const bool kDebug = false;
  printf("Playing log file %s\n", log_file.c_str());
  ScopedFile fid(log_file.c_str(), "r");
  if (fid() == NULL) {
    perror("Error opening file");
    exit(1);
  }
  // Set up UDP publisher.
  if (!publisher_.open()) {
    return;
  }

  UDPMessageWrapper message;
  uint64_t t_last_publish = 0;
  uint64_t t_last_log = 0;
  while (ReadUDPMessageWrapper(fid(), &message)) {
    printf("\r%f ", 1e-6 * static_cast<double>(message.timestamp()));
    fflush(stdout);
    if (kDebug) {
      printf("Publishing %d bytes to %s:%d\n",
             static_cast<int>(message.data().size()),
             message.address().c_str(),
             message.port());
    }
    // Wait till it is time to publish the next message.
    const int64_t delta_t_log  =
        (t_last_log > 0) ? (message.timestamp() - t_last_log) : 0;
    const int64_t delta_t_publisher =
        (t_last_publish > 0) ? (GetTimeUSec() - t_last_publish) : 0;
    const int64_t t_wait = max<int64_t>(0, delta_t_log - delta_t_publisher);
    usleep(t_wait);

    PublishMessage(message);
    t_last_publish = GetTimeUSec();
    t_last_log = message.timestamp();
  }
  printf("\n");
}

void PrintUsage() {
  printf("Usage: playback log_file.log\n");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    PrintUsage();
    return 1;
  }
  PlayLogFile(argv[1]);
  return 0;
}
