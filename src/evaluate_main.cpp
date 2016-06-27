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
// Evaluation of automatic referees by comparison to human referee.

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
#include <map>
#include <string>
#include <vector>

#include "messages_robocup_ssl_wrapper.pb.h"
#include "referee.pb.h"
#include "shared/netraw.h"
#include "shared/util.h"
#include "udp_message_wrapper.pb.h"

using std::map;
using std::max;
using std::string;
using std::vector;

// Struct to keep track of a referee events. An "event" is a stop command,
// followed by one of the following commands:
  // DIRECT_FREE_YELLOW
  // DIRECT_FREE_BLUE
  // INDIRECT_FREE_YELLOW
  // INDIRECT_FREE_BLUE
  // GOAL_YELLOW
  // GOAL_BLUE
struct RefereeEvent {
  RefereeEvent() {}

  RefereeEvent(uint64_t stop_timestamp,
               uint64_t command_timestamp,
               uint32_t command_counter,
               SSL_Referee_Command command) :
      stop_timestamp(stop_timestamp),
      command_timestamp(command_timestamp),
      command_counter(command_counter),
      command(command) {}

  // Timestamp that the "STOP" command was sent, previous to the event command.
  uint64_t stop_timestamp;

  // Timestamp that the command was sent.
  uint64_t command_timestamp;

  // Value of the command counter when this command was received.
  uint32_t command_counter;

  // Command for the event.
  SSL_Referee_Command command;
};

// UDP Multicast address for referees.
static const char* kRefereeMulticast = "224.5.23.1";

// UDP Multicast address for SSL Vision.
static const char* kVisionMulticast = "224.5.23.2";

// Port number for SSL Vision.
static const int kVisionPort = 10006;

// Port number for main refbox.
static const int kRefboxPort = 10003;

// Referee commands. The first index is for the human refbox, the rest
// are automatic referees.
vector<vector<SSL_Referee> > referee_commands;

// Referee events. The first index is for the human refbox, the rest
// are automatic referees.
vector<vector<RefereeEvent> > referee_events;

// Port numbers of referees.
vector<uint16_t> referee_ports;

// Map from referee port number, to index in referee_commands, referee_events,
// and referee_ports.
map<uint16_t, int> referee_map;

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

void PrintRefereeCommand(const int port_number,
                         const SSL_Referee& message) {
  printf("Referee %d: %4d %s\n",
         port_number,
         message.command_counter(),
         SSL_Referee_Command_Name(message.command()).c_str());
}

void LoadRefereeCommands(const string& log_file) {
  static const bool kDebug = true;
  ScopedFile fid(log_file.c_str(), "r");
  if (fid() == NULL) {
    perror("Error opening file");
    exit(1);
  }
  // Initialize map, and commands to only track human refbox first, to ensure
  // that it will correspond to the first entry in referee_commands and
  // referee_events.
  referee_map.clear();
  referee_commands.resize(1);
  referee_events.resize(1);
  referee_ports.push_back(kRefboxPort);
  referee_map[kRefboxPort] = 0;
  UDPMessageWrapper message;
  while (ReadUDPMessageWrapper(fid(), &message)) {
    if (message.address() == kVisionMulticast &&
        message.port() == kVisionPort) {
      // Parse vision message.
      SSL_DetectionFrame vision_message;
      vision_message.ParseFromString(message.data());
    } else if (message.address() == kRefereeMulticast) {
      // Referee message.
      SSL_Referee referee_message;
      referee_message.ParseFromString(message.data());

      map<uint16_t, int>::iterator it = referee_map.find(message.port());
      if (it == referee_map.end()) {
        // This referee has not bee seen before, allocate space for it.
        referee_map[message.port()] = referee_commands.size();
        referee_commands.push_back(vector<SSL_Referee>());
        referee_events.push_back(vector<RefereeEvent>());
        referee_ports.push_back(message.port());
        it = referee_map.find(message.port());
      }
      vector<SSL_Referee>& referee = referee_commands[it->second];
      if (referee.size() == 0 ||
          referee.back().command_counter() <
              referee_message.command_counter()) {
        if (kDebug) {
          PrintRefereeCommand(message.port(), referee_message);
        }
        referee.push_back(referee_message);
      }
    }
  }
  for (int i = 0; i < referee_commands.size(); ++i) {
    printf("Referee %d: %d commands\n",
            referee_ports[i],
            static_cast<int>(referee_commands[i].size()));
  }
}

void IndexRefereeEvents() {
  for (int i = 0; i < referee_commands.size(); ++i) {
    const vector<SSL_Referee>& referee = referee_commands[i];
    uint64_t t_last_stop = 0;
    for (int j = 0; j < referee.size(); ++j) {
      const SSL_Referee& command = referee[j];
      switch (command.command()) {
        case SSL_Referee_Command_STOP: {
          t_last_stop = command.command_timestamp();
        } break;

        case SSL_Referee_Command_DIRECT_FREE_YELLOW:
        case SSL_Referee_Command_DIRECT_FREE_BLUE:
        case SSL_Referee_Command_INDIRECT_FREE_YELLOW:
        case SSL_Referee_Command_INDIRECT_FREE_BLUE:
        case SSL_Referee_Command_GOAL_YELLOW:
        case SSL_Referee_Command_GOAL_BLUE: {
          referee_events[i].push_back(
              RefereeEvent(t_last_stop,
                           command.command_timestamp(),
                           command.command_counter(),
                           command.command()));
          t_last_stop = 0;
        } break;

        default: {
          // Ignore this command.
        }
      }
    }
    printf("Referee %d: %d events\n",
           referee_ports[i],
          static_cast<int>(referee_events[i].size()));
  }
}

// Returns true iff event e1 does not overlap with event e2, and the events do
// not overlap, allowing for time delay "td" before event e2.
bool Before(const RefereeEvent& e1, const RefereeEvent& e2, uint64_t td) {
  return (e1.command_timestamp < (e2.stop_timestamp -td));
}

// Returns true iff the events e1 and e2 overlap in time, allowing for time
// delay "td" before event e1.
bool Overlaps(const RefereeEvent& e1, const RefereeEvent& e2, uint64_t td) {
  return (!Before(e1, e2, 0) && !Before(e2, e1, td));
}

void EvaluateAutorefs(const string& log_file) {
  printf("Evaluating log file %s\n", log_file.c_str());
  LoadRefereeCommands(log_file);
  IndexRefereeEvents();

  // TODO: Try and load the results from possible human corrections.

  // Evaluate the autoref events.
  const vector<RefereeEvent>& human_referee = referee_events[0];
  if (human_referee.size() == 0) {
    fprintf(stderr, "ERROR: No human referee events found!\n");
    exit(1);
  }

  // The maximum time delay between an autoref event, and a human referee event
  // after the autoref event.
  static const uint64_t kAutoToHumanDelay = 2000000;
  // The maximum time delay between a human referee event, and an autoref event
  // after the human referee event.
  static const uint64_t kHumanToAutoDelay = 0;

  for (int i = 1; i < referee_events.size(); ++i) {
    const vector<RefereeEvent>& autoref = referee_events[i];
    int k = 0;
    int true_positives = 0;
    int false_positives = 0;
    int false_negatives = 0;
    for (int j = 0; j < autoref.size(); ++i) {
      // Indicates if a matching human referee command has been found.
      bool match_found = false;
      // Indicates if the autoref event has been evaluated.
      bool evaluated = false;
      do {
        if (Before(human_referee[k], autoref[j], kHumanToAutoDelay)) {
          // False negative: The autoref missed a human referee event
          ++false_negatives;
        } else if (Before(autoref[j], human_referee[k], kAutoToHumanDelay)) {
          // False Positive: No human event overlapped in time with the autoref.
          ++false_positives;
          evaluated = true;
        } else {
          // Overlapping in time.
          match_found = (human_referee[k].command == autoref[j].command);
        }
        // If no match found, check the next human referee event.
        if (!match_found) ++k;
      } while (!match_found && !evaluated && k < human_referee.size());
      if (match_found) {
        // True positive
        ++true_positives;
        // Advance to the next human referee event, since one human referee
        // event may only match one automatic referee event.
        ++k;
      } else if (!evaluated) {
        // False positive. There are no more human referee events left.
        ++false_positives;
      }
    }
    const float precision =
        static_cast<float>(true_positives) /
        (static_cast<float>(true_positives) +
        static_cast<float>(false_positives));
    const float recall =
        static_cast<float>(true_positives) /
        (static_cast<float>(true_positives) +
        static_cast<float>(false_negatives));
    const float f1_score = 2.0 * precision * recall / (precision + recall);
    printf("Autoref %d:\n"
           "True Positives: %d\n"
           "False Positives: %d\n"
           "False Negatives: %d\n"
           "Precision: %.3f\n"
           "Recall: %.3f\n"
           "F1 Score: %.3f\n",
           referee_ports[i],
           true_positives,
           false_positives,
           false_negatives,
           precision,
           recall,
           f1_score);
    // TODO: Save the results for possible human correction.
  }
}

void PrintUsage() {
  printf("Usage: evaluate log_file.log\n");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    PrintUsage();
    return 1;
  }
  EvaluateAutorefs(argv[1]);
  return 0;
}
