// file.cc
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

#include <fstream>
#include <cstdarg>
#include <iterator>
#include "file.h"
#include "stringutil.h"
#include "util.h"

namespace trainc {

File::File(const std::string &filename, const char *mode)
  : mode_(static_cast<std::ios_base::openmode>(0)) {
  Open(filename, mode);
}

File::~File() {
  Close();
}

bool File::Open(const std::string &filename, const char *mode) {
  Close();
  mode_ = (std::string("w") == mode ? std::ios_base::out : std::ios_base::in);
  stream_.open(filename.c_str(), mode_);
  return stream_.is_open() && !stream_.fail();
}

void File::Close() {
  stream_.close();
}

bool File::IsOpen() const {
  return stream_.is_open();
}

bool File::IsEof() const {
  return stream_.eof();
}

bool File::IsReader() const {
  return mode_ & std::ios_base::in;
}

bool File::IsWriter() const {
  return mode_ & std::ios_base::out;
}

std::fstream& File::Stream() {
  return stream_;
}

File* File::Create(const std::string &filename, const char *mode) {
  File *f = new File(filename, mode);
  if (!f->IsOpen()) {
    delete f;
    f = NULL;
  }
  return f;
}

File* File::OpenOrDie(const std::string &filename, const char *mode) {
  File *f = Create(filename, mode);
  if (!f) {
    LOG(FATAL) << "cannot open " << filename;
  }
  return f;
}

std::string File::ReadFileToStringOrDie(const std::string &filename) {
  std::string result;
  ReadFileToStringOrDie(filename, &result);
  return result;
}

void File::ReadFileToStringOrDie(const std::string &filename, std::string *content) {
  InputBuffer ib(OpenOrDie(filename, "r"));
  content->clear();
  ib.ReadToString(content);
}

void File::Printf(const char *format, ...) {
  CHECK(IsWriter());
  va_list ap;
  va_start(ap, format);
  std::string s = StringVPrintf(format, ap);
  va_end(ap);
  stream_ << s;
}

// =======================================================================

InputBuffer::InputBuffer(File *file, int) : FileBuffer(file) {
  CHECK(file->IsReader());
}

bool InputBuffer::ReadLine(std::string *line) {
  if (!file_->IsOpen() || file_->IsEof()) {
    return false;
  }
  std::getline(file_->Stream(), *line);
  return !file_->Stream().fail();
}

bool InputBuffer::ReadToString(std::string *str) {
  if (!file_->IsOpen() || file_->IsEof()) {
    return false;
  }
  str->clear();
  str->insert(str->begin(),
      std::istreambuf_iterator<char>(file_->Stream().rdbuf()),
      std::istreambuf_iterator<char>());
  return !file_->Stream().fail();
}

// =======================================================================

const size_t OutputBuffer::kBufferSize = 8096;

OutputBuffer::OutputBuffer(File *file)
  : FileBuffer(file), buffer_(new char[kBufferSize]), pos_(0) {
  CHECK(file->IsWriter());
}

OutputBuffer::~OutputBuffer() {
  if (file_->IsOpen()) {
    CloseFile();
  }
  delete[] buffer_;
}

void OutputBuffer::WriteString(const std::string &string) {
  Buffer(string.c_str(), string.size());
}

void OutputBuffer::WriteString(const char *string, size_t n) {
  Buffer(string, n);
}

void OutputBuffer::Buffer(const char *data, size_t n) {
  while (pos_ + n > kBufferSize) {
    const char *data_end = data + (kBufferSize - pos_);
    std::copy(data, data_end, buffer_ + pos_);
    n -= (kBufferSize - pos_);
    data = data_end;
    pos_ = kBufferSize;
    Flush();
  }
  if (n > 0) {
    std::copy(data, data + n, buffer_ + pos_);
    pos_ += n;
  }
}

void OutputBuffer::Flush() {
  if (pos_)
    file_->Stream().write(buffer_, pos_);
  file_->Stream().flush();
  pos_ = 0;
}

bool OutputBuffer::CloseFile() {
  bool ok = file_->Stream().good();
  Flush();
  file_->Close();
  return ok;
}

}  // namespace trainc
