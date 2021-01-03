
#include "stdafx.h"
#include "Array_String.h"
#include "Globals.h"
#include <glm/glm.hpp>
Array_String::Array_String()
{
  str[0] = '\0';
}
Array_String::Array_String(const std::string &rhs)
{
  const char *cstr = rhs.c_str();
  uint32 len = strlen(cstr);
  ASSERT(len <= MAX_ARRAY_STRING_LENGTH);
  strcpy_s(&str[0], MAX_ARRAY_STRING_LENGTH + 1, cstr);
}
bool Array_String::operator==(Array_String& rhs)
{
  for (uint32 i = 0; i < MAX_ARRAY_STRING_LENGTH; ++i)
  {
    if (str[i] == '\0')
    {
      if (rhs.str[i] == '\0')
        return true;
    }
    if (str[i] != rhs.str[i])
      return false;
  }
  return true;
}
Array_String& Array_String::operator=(std::string& rhs)
{
  ASSERT(rhs.size() <= MAX_ARRAY_STRING_LENGTH);
  SDL_strlcpy(&str[0], &rhs[0], rhs.size() + 1);
  return *this;
}

bool Array_String::operator==(const char* rhs)
{
  return strncmp(&str[0], rhs, MAX_ARRAY_STRING_LENGTH + 1) == 0;
}

Array_String::operator const char *()
{
  return &str[0];
}

Array_String::Array_String(const char *rhs)
{
  uint32 len = strnlen_s(rhs, MAX_ARRAY_STRING_LENGTH + 1);
  ASSERT(len <= MAX_ARRAY_STRING_LENGTH);
  strcpy(&str[0], rhs);
}
