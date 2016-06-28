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

#include <stdint.h>

#include <string>

#ifndef MISC_UTIL_H_
#define MISC_UTIL_H_

// A simple class wrapper for a file handle that automatically closes the file
// when the instance of the class is destroyed.
class ScopedFile {
  static const bool kDebug = false;
 public:
  // Constructor that inherits ownership of a previously opened file.
  explicit ScopedFile(FILE* fid);

  // Constructor that opens the specified file in the specified mode.
  ScopedFile(const std::string& file_name,
             const char* mode,
             bool print_error = false);

  // Destructor.
  ~ScopedFile();

  // Open a file explicitly.
  void Open(const std::string& file_name,
            const char* mode,
            bool print_error = false);

  // Getter for the underlying file handle.
  FILE* operator()();

  // Conversion operator to convert to FILE* type.
  operator FILE*&();

 private:
  // Disable the default constructor.
  ScopedFile();

  // Disable the copy constructor.
  ScopedFile(const ScopedFile& other);

  // Disable the assignment operator.
  const ScopedFile& operator=(const ScopedFile& other);

private:
  // The file handle owned by this instance.
  FILE* fid_;
};

// Get timestamp as reported by gettimeofday, in number of microseconds
uint64_t GetTimeUSec();

// Returns truee iff the file specified exists in the file system.
bool FileExists(const std::string& file_name);

// Return an std::string created using a printf-like syntax.
std::string StringPrintf(const char* format, ...)
    __attribute__((format(__printf__,1,2)));

#endif  // MISC_UTIL_H_
