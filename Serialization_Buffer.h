#include "stdafx.h"
#include "Globals.h"

struct Serialization_Buffer
{

// BUFFER_DEBUG will push a size for every insert, and pop a size for every read
// and assert that the sizes match
#define BUFFER_DEBUG 1

  template <typename T> void push(T d)
  {
    const uint32 size = sizeof(T);

#if BUFFER_DEBUG
    const uint32 debug_end = data.size();
    data.resize(data.size() + sizeof(uint32));
    memcpy(&data[debug_end], &debug_end, sizeof(uint32));
#endif

    const uint32 end = data.size();
    data.resize(data.size() + size);
    memcpy(&data[end], &d, size);
  }
  template <> void push(const char *s) = delete;

  template <> void push(std::string s)
  {
    const uint32 length = s.size();
    push(length);

    const uint32 end = data.size();
    data.resize(data.size() + length);
    memcpy(&data[end], &s[0], length);
  }

  template <typename T> void read(T *t)
  {
    ASSERT(t);
    const uint32 size = sizeof(T);
    read(t, size);
  }
  template <> void read(const char *s) = delete;
  template <> void read(std::string *dst)
  {
    ASSERT(dst);
    uint16 length = 0;
    read(&length);

    dst->resize(length);
    ASSERT(read_index + length <= data.size());
    uint8 *const write_ptr = (uint8 *)&dst[0];
    for (uint32 i = 0; i < length; ++i)
    {
      *(write_ptr + i) = data[read_index];
      ++read_index;
    }
  }

  void read(void *dst, size_t size)
  {
#if BUFFER_DEBUG
    ASSERT(read_index + sizeof(uint32) <= data.size());
    uint32 debug_size;
    uint8 *const debug_ptr = (uint8 *)&debug_size;
    for (uint32 i = 0; i < sizeof(uint32); ++i)
    {
      *(debug_ptr + i) = data[read_index];
      ++read_index;
    }
    ASSERT(debug_size == size);
#endif

    ASSERT(read_index + size <= data.size());
    uint8 *write_ptr = (uint8 *)dst;
    for (uint32 i = 0; i < size; ++i)
    {
      *(write_ptr + i) = data[read_index];
      ++read_index;
    }
  }

private:
  std::vector<uint8_t> data;
  uint32 read_index = 0;
};
