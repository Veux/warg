#pragma once
#include "Globals.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>
using namespace glm;
struct LightCache
{
  GLint position, direction, color, attenuation, ambient, cone_angle, type;
};
struct Shader
{
  Shader();
  Shader(const std::string &vertex, const std::string &fragment);
  void load(const std::string &vertex, const std::string &fragment);

  void set_uniform(const char *name, uint32 i);
  void set_uniform(const char *name, int32 i);
  void set_uniform(const char *name, float32 f);
  void set_uniform(const char *name, vec2 v);
  void set_uniform(const char *name, vec3 &v);
  void set_uniform(const char *name, vec4 &v);
  void set_uniform(const char *name, const mat4 &m);
  GLint get_uniform_location(const char *name);

  void use() const;

  struct Shader_Handle
  {
    Shader_Handle(GLuint i);
    ~Shader_Handle();
    GLuint program = 0;
  };
  std::shared_ptr<Shader_Handle> program;
  std::string vs;
  std::string fs;
  std::unordered_map<std::string, GLint> location_cache;
  LightCache lights_cache[MAX_LIGHTS] = {0};
  bool cache_set = false;

private:
  void check_err(GLint loc, const char *name);

  // todo: create create every uniform as an int =-1 and assign their binding
  // locations to avoid get_uniform_location spam
};
