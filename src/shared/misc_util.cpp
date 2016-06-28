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
// Miscellaneous helper functions and classes.

#include "misc_util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

ScopedFile::ScopedFile(FILE* fid) : fid_(fid) {}

ScopedFile::ScopedFile(const std::string& file_name,
            const char* mode,
            bool print_error) : fid_(NULL) {
  Open(file_name, mode, print_error);
}

ScopedFile::~ScopedFile() {
  if (fid_ == NULL) return;
  const bool error = (fclose(fid_) != 0);
  if (kDebug) {
    printf("fclose success:%d\n", error);
    if (error) perror("Error closing file descriptor");
  }
}

void ScopedFile::Open(const std::string& file_name,
                      const char* mode,
                      bool print_error) {
  if (fid_) fclose(fid_);
  fid_ = fopen(file_name.c_str(), mode);
  if (fid_ == NULL) {
    if (print_error) {
      const std::string error_string = "Error opening \"" + file_name + "\"";
      perror(error_string.c_str());
    }
  } else if (kDebug){
    printf("fopen: 0x%08X\n", fileno(fid_));
  }
}

FILE* ScopedFile::operator()() { return (fid_); }

ScopedFile::operator FILE*&() { return (fid_); }

uint64_t GetTimeUSec() {
  timespec time;
  clock_gettime(CLOCK_REALTIME, &time);
  const uint64_t seconds = time.tv_sec;
  const uint64_t useconds = time.tv_nsec / 1000;
  return (seconds * 1000000 + useconds);
}

bool FileExists(const std::string& file_name) {
  struct stat st;
  return(stat(file_name.c_str(), &st) == 0);
}

std::string StringPrintf(const char* format, ...) {
  va_list al;
  int string_length = 0;
  char* buffer = NULL;

  va_start(al,format);
  string_length = vsnprintf(buffer,string_length,format,al);
  va_end(al);
  if (string_length == 0) return (std::string());
  buffer = new char[string_length + 1];

  va_start(al,format);
  string_length = vsnprintf(buffer,string_length + 1,format,al);
  va_end(al);
  const std::string return_string(buffer);
  delete[] buffer;
  return (return_string);
}
