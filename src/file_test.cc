// file_test.cc
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
// Unit tests for File, InputBuffer, OutputBuffer

#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>
#include "file.h"
#include "unittest.h"
#include "util.h"

namespace trainc {

class TestFile : public ::testing::Test {
public:
  void SetUp() {
    filename_ = ::tempnam(NULL, NULL);
    ASSERT_NOTNULL(filename_);
  }

  void TearDown() {
    ::unlink(filename_);
  }

  void Create() {
    File file(filename_, "w");
    EXPECT_FALSE(file.IsReader());
    EXPECT_TRUE(file.IsWriter());
    file.Close();
    EXPECT_EQ(::access(filename_, F_OK), 0);
  }

  void CreateFail() {
    File file("/dev/null/X", "w");
    EXPECT_FALSE(file.IsOpen());
  }

  void ReadFail() {
    File file(filename_, "r");
    EXPECT_FALSE(file.IsOpen());
  }

  void Read() {
    {
      File file(filename_, "w");
    }
    File file(filename_, "r");
    EXPECT_TRUE(file.IsOpen());
  }

  void ReadWrite() {
    const std::string testString = "test";
    WriteToFile(testString);
    File *file = File::Create(filename_, "r");
    ASSERT_NOTNULL(file);
    InputBuffer i(file);
    std::string s;
    i.ReadToString(&s);
    EXPECT_EQ(s, testString);
  }

  void Printf() {
    {
      File *file = File::Create(filename_, "w");
      ASSERT_NOTNULL(file);
      file->Printf("%s %d", "test", 1234);
      file->Close();
      delete file;
    }
    File *file = File::Create(filename_, "r");
    ASSERT_NOTNULL(file);
    InputBuffer i(file);
    std::string s;
    i.ReadToString(&s);
    EXPECT_EQ(s, std::string("test 1234"));
  }


  void ReadLine() {
    const char* testString[] = { "test", "line" };
    {
      File *file = File::Create(filename_, "w");
      ASSERT_NOTNULL(file);
      OutputBuffer o(file);
      for (int i = 0; i < 2; ++i)
        o.WriteString(std::string(testString[i]) + "\n");
    }
    File *file = File::Create(filename_, "r");
    ASSERT_NOTNULL(file);
    InputBuffer in(file);
    std::string s;
    for (int i = 0; i < 2; i++) {
      s.clear();
      bool r = in.ReadLine(&s);
      EXPECT_TRUE(r);
      EXPECT_EQ(s, std::string(testString[i]));
    }
    bool r = in.ReadLine(&s);
    EXPECT_FALSE(r);
  }

  void WriteLarge() {
    std::string str;
    for (int i = 0; i < int(8096 * 2.5); i++)
      str += ('a' + (i % 26));
    WriteToFile(str);
    File *file = File::Create(filename_, "r");
    ASSERT_NOTNULL(file);
    InputBuffer in(file);
    std::string s;
    in.ReadToString(&s);
    EXPECT_EQ(s, str);
  }

  void BinaryData() {
    int testInt = 8;
    float testFloat = 3.67;
    {
      File *file = File::Create(filename_, "w");
      ASSERT_NOTNULL(file);
      OutputBuffer o(file);
      o.WriteBinary(testInt);
      o.WriteBinary(testFloat);
    }
    File *file = File::Create(filename_, "r");
    ASSERT_NOTNULL(file);
    InputBuffer in(file);
    int readInt;
    float readFloat;
    bool r = in.ReadBinary(&readInt);
    EXPECT_TRUE(r);
    EXPECT_EQ(readInt, testInt);
    r = in.ReadBinary(&readFloat);
    EXPECT_EQ(readFloat, testFloat);
    EXPECT_TRUE(r);
    r = in.ReadBinary(&readInt);
    EXPECT_FALSE(r);
  }

  void TextData() {
    int testInt = 8;
    float testFloat = 3.67;
    {
      File *file = File::Create(filename_, "w");
      ASSERT_NOTNULL(file);
      OutputBuffer o(file);
      o.WriteText(testInt);
      o.WriteString("\t");
      o.WriteText(testFloat);
    }
    File *file = File::Create(filename_, "r");
    ASSERT_NOTNULL(file);
    InputBuffer in(file);
    int readInt;
    float readFloat;
    bool r = in.ReadText(&readInt);
    EXPECT_TRUE(r);
    EXPECT_EQ(readInt, testInt);
    r = in.ReadText(&readFloat);
    EXPECT_EQ(readFloat, testFloat);
    EXPECT_TRUE(r);
    r = in.ReadText(&readInt);
    EXPECT_FALSE(r);
  }

protected:
  void WriteToFile(const std::string &s) {
    File *file = File::Create(filename_, "w");
    ASSERT_NOTNULL(file);
    OutputBuffer o(file);
    o.WriteString(s);
  }

protected:
  char *filename_;
};

TEST_F(TestFile, Create) {
  Create();
}

TEST_F(TestFile, CreateFail) {
  CreateFail();
}

TEST_F(TestFile, ReadFail) {
  ReadFail();
}

TEST_F(TestFile, Read) {
  Read();
}

TEST_F(TestFile, ReadWrite) {
  ReadWrite();
}

TEST_F(TestFile, ReadLine) {
  ReadLine();
}

TEST_F(TestFile, WriteLarge) {
  WriteLarge();
}

TEST_F(TestFile, BinaryData) {
  BinaryData();
}

TEST_F(TestFile, TextData) {
  TextData();
}

TEST_F(TestFile, Printf) {
  Printf();
}

}  // namespace trainc
