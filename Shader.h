#pragma once
#include "Globals.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>
using namespace glm;
enum struct Light_Type;
struct Light_Uniform_Location_Cache
{
  GLint position, direction, color, attenuation, ambient, cone_angle, type, enabled, shadow_map_transform;
};
struct Light_Uniform_Value_Cache
{
  vec3 position, direction, color, attenuation, ambient;
  float cone_angle;
  Light_Type type;
  mat4 shadow_map_transform;
  bool enabled;
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
    std::unordered_map<std::string, GLint> location_cache;
    Light_Uniform_Value_Cache light_values_cache[MAX_LIGHTS] = {};
    Light_Uniform_Location_Cache light_locations_cache[MAX_LIGHTS] = {};
	  GLuint light_count_location, additional_ambient_location;
    uint32 light_count = 0;
    vec3 additional_ambient = vec3(0);
    void set_location_cache();
    bool light_location_cache_set = false;
  };
  std::shared_ptr<Shader_Handle> program;
  std::string vs;
  std::string fs;

private:
  void check_err(GLint loc, const char *name);

  // todo: create create every uniform as an int =-1 and assign their binding
  // locations to avoid get_uniform_location spam
};
