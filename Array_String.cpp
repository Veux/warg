#pragma once
#include "stdafx.h"
#include "Array_String.h"
#include "Globals.h"
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
Array_String::Array_String()
{
  str[0] = '\0';
}
Array_String::Array_String(std::string &rhs)
{
  ASSERT(rhs.size() <= MAX_ARRAY_STRING_LENGTH);
  SDL_strlcpy(&str[0], &rhs[0], rhs.size() + 1);
}
bool Array_String::operator==(Array_String &rhs)
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
Array_String &Array_String::operator=(std::string &rhs)
{
  ASSERT(rhs.size() <= MAX_ARRAY_STRING_LENGTH);
  SDL_strlcpy(&str[0], &rhs[0], rhs.size() + 1);
  return *this;
}

bool Array_String::operator==(const char *rhs)
{
  Array_String r = s(rhs);
  return *this == r;
}

Array_String &Array_String::operator=(const char *rhs)
{
  *this = std::string(rhs);
  return *this;
}

Array_String::Array_String(const char *rhs) : Array_String(std::string(rhs)) {}