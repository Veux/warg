#pragma once

#define SIM_LATENCY 100

#include "Spell.h"
#include "Warg_Common.h"
#include <enet/enet.h>
#include <map>

const int32 SPAWN_MESSAGE = 1;
const int32 MOVE_MESSAGE = 2;
const int32 CAST_MESSAGE = 3;
const int32 STATE_MESSAGE = 4;
const int32 CHAT_MESSAGE = 5;
const int32 CHAT_MESSAGE_RELAY = 6;

struct Buffer
{
  void reserve(size_t size);
  void insert(void *d, size_t size);
  void *read(size_t size);

  std::vector<uint8_t> data;
  size_t wnext = 0, rnext = 0;
};

void serialize_(Buffer &b, std::string &s);
void serialize_(Buffer &b, int32_t n);
void serialize_(Buffer &b, uint32_t n);
void serialize_(Buffer &b, float64 a);
void serialize_(Buffer &b, std::string_view s);
void serialize_(Buffer &b, quat q);
void serialize_(Buffer &b, Game_State &gs);
void deserialize(Buffer &b, uint8 &n);
void deserialize(Buffer &b, uint16 &n);
void deserialize(Buffer &b, Game_State &gs);
void deserialize(Buffer &b, int32 &n);
void deserialize(Buffer &b, std::string &s);
void deserialize(Buffer &b, quat &q);
void deserialize(Buffer &b, uint32 &n);
void deserialize(Buffer &b, float64 &x);