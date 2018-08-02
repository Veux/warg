#include "stdafx.h"
#include "Globals.h"

struct Serialization_Buffer
{

// BUFFER_DEBUG will push a size for every insert, and pop a size for every read
// and assert that the sizes match
#define BUFFER_DEBUG 1

  // push:
  template <typename T, typename = std::enable_if_t<std::is_fundamental<T>>> void push(T d)
  {
    const uint8 size = sizeof(T);
#if BUFFER_DEBUG
    const uint32 debug_end = data.size();
    data.resize(data.size() + sizeof(uint8));
    memcpy(&data[debug_end], &debug_end, sizeof(uint8));
#endif
    const uint32 end = data.size();
    data.resize(data.size() + size);
    memcpy(&data[end], &d, size);
  }

  template <> void push(std::string s)
  {
    const uint32 length = s.size();
    push(length);

    const uint32 end = data.size();
    data.resize(data.size() + length);
    memcpy(&data[end], &s[0], length);
  }
  template <> void push(vec2 v)
  {
    push(v.x);
    push(v.y);
  }
  template <> void push(vec3 v)
  {
    push(v.x);
    push(v.y);
    push(v.z);
  }
  template <> void push(vec4 v)
  {
    push(v.x);
    push(v.y);
    push(v.z);
    push(v.w);
  }
  template <> void push(ivec2 v)
  {
    push(v.x);
    push(v.y);
  }
  template <> void push(ivec3 v)
  {
    push(v.x);
    push(v.y);
    push(v.z);
  }
  template <> void push(ivec4 v)
  {
    push(v.x);
    push(v.y);
    push(v.z);
    push(v.w);
  }

  // read:
  template <typename T, typename = std::enable_if_t<std::is_fundamental<T>>> void read(T *t)
  {
    ASSERT(t);
    const uint32 size = sizeof(T);
    _read(t, size);
  }

  template <> void read(std::string *dst)
  {
    ASSERT(dst);
    uint16 length = 0;
    read(&length);

    dst->resize(length);
    // cannot use _read because of the debug size assert
    ASSERT(read_index + length <= data.size());
    uint8 *const write_ptr = (uint8 *)&dst[0];
    for (uint32 i = 0; i < length; ++i)
    {
      *(write_ptr + i) = data[read_index];
      ++read_index;
    }
  }
  template <> void read(vec2 *v)
  {
    read(&v->x);
    read(&v->y);
  }
  template <> void read(vec3 *v)
  {
    read(&v->x);
    read(&v->y);
    read(&v->z);
  }
  template <> void read(vec4 *v)
  {
    read(&v->x);
    read(&v->y);
    read(&v->z);
    read(&v->w);
  }
  template <> void read(ivec2 *v)
  {
    read(&v->x);
    read(&v->y);
  }
  template <> void read(ivec3 *v)
  {
    read(&v->x);
    read(&v->y);
    read(&v->z);
  }
  template <> void read(ivec4 *v)
  {
    read(&v->x);
    read(&v->y);
    read(&v->z);
    read(&v->w);
  }

private:
  void _read(void *dst, size_t size)
  {
#if BUFFER_DEBUG
    ASSERT(read_index + sizeof(uint8) <= data.size());
    uint8 debug_size;
    debug_size = data[read_index];
    ++read_index;
    ASSERT(debug_size == size);
#endif

    ASSERT(read_index + size <= data.size());
    uint8 *const write_ptr = (uint8 *)dst;
    for (uint32 i = 0; i < size; ++i)
    {
      *(write_ptr + i) = data[read_index];
      ++read_index;
    }
  }

  std::vector<uint8> data;
  uint32 read_index = 0;
};
