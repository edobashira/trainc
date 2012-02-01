// file.h
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2010 RWTH Aachen University. All Rights Reserved.
// Author: rybach@cs.rwth-aachen.de (David Rybach)
//
// \file
// File stream handling

#ifndef FILE_H_
#define FILE_H_

#include <fstream>
#include <sstream>

namespace trainc {

// Wrapper class for a file stream object.
class File {
public:
  File(const std::string &filename, const char *mode);
  ~File();

  // for compatibility with Google's File
  static void Init() {}
  static File* Create(const std::string &filename, const char *mode);
  static File* OpenOrDie(const std::string &filename, const char *mode);
  static std::string ReadFileToStringOrDie(const std::string &filename);
  static void ReadFileToStringOrDie(const std::string &filename, std::string *content);

  bool Open(const std::string &filename, const char *mode);
  // for compatibility with Google's File
  bool Open() {
    return IsOpen();
  }
  void Close();

  bool IsOpen() const;
  bool IsEof() const;

  bool IsReader() const;
  bool IsWriter() const;

  void Printf(const char *format, ...);

  std::fstream& Stream();

protected:
  std::ios_base::openmode mode_;
  std::fstream stream_;
};


// Base class for InputBuffer and OutputBuffer.
// Handles the File object.
class FileBuffer {
public:
  explicit FileBuffer(File *file) : file_(file) {}
  virtual ~FileBuffer() { delete file_; }
  virtual bool CloseFile() { return true; }
protected:
  File *file_;
};


// File input operations.
class InputBuffer : public FileBuffer {
public:
  // Create InputBuffer using the given File object.
  // Takes ownership of the File object.
  explicit InputBuffer(File *file, int i = 0);
  virtual ~InputBuffer() {}
  // Read one line.
  bool ReadLine(std::string *line);
  // Read the complete file.
  bool ReadToString(std::string *str);
  // Read from binary data.
  template<class T>
  bool ReadBinary(T *t) {
    file_->Stream().read(reinterpret_cast<char*>(t), sizeof(T));
    return !file_->Stream().fail();
  }
  // Read from text data.
  template<class T>
  bool ReadText(T *t) {
    file_->Stream() >> *t;
    return !file_->Stream().fail();
  }
};

// Buffered file output operations.
class OutputBuffer : public FileBuffer {
public:
  // Create OutputBuffer using the given File object.
  // Takes ownership of the File object.
  explicit OutputBuffer(File *file);
  virtual ~OutputBuffer();
  // Write buffered data to the file.
  void Flush();
  // Close the underlying file object.
  // Calls Flush().
  bool CloseFile();
  // Write string to the file.
  void WriteString(const std::string &string);
  // Write string to the file.
  void WriteString(const char *string, size_t len);
  // Write object to a binary file.
  template<class T>
  void WriteBinary(const T &t) {
    Buffer(reinterpret_cast<const char*>(&t), sizeof(t));
  }
  // Write object to a text file.
  template<class T>
  void WriteText(const T &t) {
    std::stringstream buf;
    buf << t;
    Buffer(buf.str().c_str(), buf.str().size());
  }

protected:
  void Buffer(const char *data, size_t n);
  static const size_t kBufferSize;
  char *buffer_;
  size_t pos_;
};

}  // namespace trainc

#endif  // FILE_H_
