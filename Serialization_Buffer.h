#pragma once
#include "Globals.h"
struct Serialization_Buffer
{
  Serialization_Buffer() {}
  Serialization_Buffer(uint8 *src, size_t length)
  {
    data.resize(length);
    memcpy(&data[0], src, length);
  }

// BUFFER_DEBUG will push a size for every insert, and pop a size for every read
// and assert that the sizes match, an attempted read into a dst type that is the
// wrong size will cause an error
#define BUFFER_DEBUG 1

  // push a fundamental type:
  template <typename T, typename = std::enable_if_t<std::is_fundamental<T>::value>> void push(T d)
  {
    const uint32 size = sizeof(T);
#if BUFFER_DEBUG
    ASSERT(sizeof(T) <= UINT32_MAX);
    const size_t debug_end = data.size();
    data.resize(data.size() + sizeof(uint32));
    memcpy(&data[debug_end], &size, sizeof(uint32));
#endif
    const size_t end = data.size();
    data.resize(data.size() + size);
    memcpy(&data[end], &d, size);
  }

  void push(std::string s)
  {
    const uint16 length = (uint16)s.size();
    push(length);

    const size_t end = data.size();
    data.resize(data.size() + length);
    memcpy(&data[end], &s[0], length);
  }
  void push(glm::vec2 v)
  {
    push(v.x);
    push(v.y);
  }
  void push(vec3 v)
  {
    push(v.x);
    push(v.y);
    push(v.z);
  }
  void push(vec4 v)
  {
    push(v.x);
    push(v.y);
    push(v.z);
    push(v.w);
  }
  void push(ivec2 v)
  {
    push(v.x);
    push(v.y);
  }
  void push(ivec3 v)
  {
    push(v.x);
    push(v.y);
    push(v.z);
  }
  void push(ivec4 v)
  {
    push(v.x);
    push(v.y);
    push(v.z);
    push(v.w);
  }
  void push(quat v)
  {
    push(v.x);
    push(v.y);
    push(v.z);
    push(v.w);
  }

  template <typename T, typename = std::enable_if_t<std::is_fundamental<T>::value>> void read(T *t)
  {
    ASSERT(t);
    const uint32 size = sizeof(T);
    _read(t, size);
  }

  template <typename T, typename = std::enable_if_t<std::is_fundamental<T>::value>> void peek(T *t)
  {
    ASSERT(t);
    const uint32 size = sizeof(T);
    _peek(t, size);
  }

  void read(std::string *dst)
  {
    ASSERT(dst);
    uint16 length = 0;
    read(&length);

    dst->resize(length);
    // cannot use _read because of the debug size assert
    ASSERT(read_index + length <= data.size());
    
    uint8 *const write_ptr = (uint8*) &((*dst)[0]);
    memcpy(write_ptr, &data[read_index], length);
    read_index += length;
  }
  void read(vec2 *v)
  {
    read(&v->x);
    read(&v->y);
  }
  void read(vec3 *v)
  {
    read(&v->x);
    read(&v->y);
    read(&v->z);
  }
  void read(vec4 *v)
  {
    read(&v->x);
    read(&v->y);
    read(&v->z);
    read(&v->w);
  }
  void read(ivec2 *v)
  {
    read(&v->x);
    read(&v->y);
  }
  void read(ivec3 *v)
  {
    read(&v->x);
    read(&v->y);
    read(&v->z);
  }
  void read(ivec4 *v)
  {
    read(&v->x);
    read(&v->y);
    read(&v->z);
    read(&v->w);
  }
  void read(quat *v)
  {
    read(&v->x);
    read(&v->y);
    read(&v->z);
    read(&v->w);
  }
  void reset_read_pointer()
  {
    read_index = 0;
  }

  std::vector<uint8> data;

private:
  void _read(void *dst, size_t size)
  {
#if BUFFER_DEBUG
    ASSERT(read_index + sizeof(uint32) <= data.size());
    uint32 debug_size;
    memcpy(&debug_size, &data[read_index], sizeof(uint32));
    ASSERT(debug_size == size);
    read_index += sizeof(uint32);
#endif
    ASSERT(read_index + size <= data.size());
    uint8 *const write_ptr = (uint8 *)dst;
    if (write_ptr)
      memcpy(write_ptr, &data[read_index], size);
    read_index += size;
  }

  void _peek(void *dst, size_t size)
  {
    uint32 peek_index = read_index;
#if BUFFER_DEBUG
    ASSERT(read_index + sizeof(uint8) <= data.size());
    uint32 debug_size;
    memcpy(&debug_size, &data[read_index], sizeof(uint32));
    ASSERT(debug_size == size);
    peek_index += sizeof(uint32);
#endif
    ASSERT(read_index + size <= data.size());
    uint8 *const write_ptr = (uint8 *)dst;
    if (write_ptr)
      memcpy(write_ptr, &data[peek_index], size);
  }
  uint32 read_index = 0;
};
