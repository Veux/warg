#pragma once
#include <string>
#include <array>
#define MAX_ARRAY_STRING_LENGTH 512
struct Array_String
{
  Array_String();
  Array_String(const std::string& rhs);
  Array_String& operator=(std::string& rhs);
  Array_String(const char* rhs);
  Array_String& operator=(const char* rhs);
  bool operator==(Array_String& rhs);
  bool operator==(const char* rhs);
  operator const char* ();
  std::array<char, MAX_ARRAY_STRING_LENGTH + 1> str;
};


