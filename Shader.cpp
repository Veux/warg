#include "stdafx.h"
#include "Shader.h"
#include "Globals.h"
#include "Render.h"

std::unordered_map<std::string, std::weak_ptr<Shader_Handle>> SHADER_CACHE;

GLuint load_shader(const std::string &vertex_path, const std::string &fragment_path)
{
  check_gl_error();
  std::string full_vertex_path = BASE_SHADER_PATH + vertex_path;
  std::string full_fragment_path = BASE_SHADER_PATH + fragment_path;
  GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
  GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
  std::string vs = read_file(full_vertex_path.c_str());
  std::string fs = read_file(full_fragment_path.c_str());
  const char *vert = vs.c_str();
  const char *frag = fs.c_str();
  GLint result = 0;
  int logLength;
  bool success = true;
  set_message("Loading shader: ", s(vertex_path, " ", fragment_path));
  // set_message("Compiling vertex shader: ", vertex_path);
  // set_message("Vertex Shader Source: \n", vs);
  glShaderSource(vert_shader, 1, &vert, NULL);
  glCompileShader(vert_shader);
  glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &result);
  if (!result)
    success = false;
  glGetShaderiv(vert_shader, GL_INFO_LOG_LENGTH, &logLength);
  std::vector<GLchar> vertShaderError((logLength > 1) ? logLength : 1);
  glGetShaderInfoLog(vert_shader, logLength, NULL, &vertShaderError[0]);
  if (vertShaderError.size() != 1)
    set_message(s("Vertex shader ", vertex_path, " compilation result: "), &vertShaderError[0],30.f);

  // set_message("Compiling fragment shader: ", fragment_path);
  // set_message("Fragment Shader Source: \n", fs);
  glShaderSource(frag_shader, 1, &frag, NULL);
  glCompileShader(frag_shader);
  glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &result);
  if (!result)
    success = false;
  glGetShaderiv(frag_shader, GL_INFO_LOG_LENGTH, &logLength);
  std::vector<GLchar> fragShaderError((logLength > 1) ? logLength : 1);
  glGetShaderInfoLog(frag_shader, logLength, NULL, &fragShaderError[0]);

  if (fragShaderError.size() != 1)
    set_message(s("Fragment shader ", fragment_path, " compilation result: "), &fragShaderError[0],30.f);

  // set_message("Linking shaders");
  GLuint program = glCreateProgram();
  glAttachShader(program, vert_shader);
  glAttachShader(program, frag_shader);
  glLinkProgram(program);
  glGetProgramiv(program, GL_LINK_STATUS, &result);
  if (!result)
    success = false;
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
  std::vector<char> err((logLength > 1) ? logLength : 1);
  glGetProgramInfoLog(program, logLength, NULL, &err[0]);

  if (err.size() != 1)
    set_message(s("GL Shader linker output for: ", vertex_path, " ", fragment_path), &err[0],30.f);

  glDeleteShader(vert_shader);
  glDeleteShader(frag_shader);

  if (!success)
  {
    set_message("GL Shader failed.");
    return 0;
    //ASSERT(0);
  }
  set_message("Shader loaded successfully");

  check_gl_error();
  return program;
}

Shader_Handle::Shader_Handle(GLuint i) { program = i; }
Shader_Handle::~Shader_Handle() { glDeleteProgram(program); }
Shader::Shader() {}
Shader::Shader(const std::string &vertex, const std::string &fragment) { load(vertex.c_str(), fragment.c_str()); }

void Shader::load(const std::string &vertex, const std::string &fragment)
{
  std::string key = vertex;
  key.append(fragment);

  auto ptr = SHADER_CACHE[key].lock();
  if (!ptr)
  {
    program = ptr = std::make_shared<Shader_Handle>(load_shader(vertex, fragment));
    SHADER_CACHE[key] = ptr;
    // set_message("Caching light uniform locations");
    program->vs = vs = std::string(vertex);
    program->fs = fs = std::string(fragment);
    use();
    program->set_location_cache();
  }
  else
  {
    program = ptr;
    program->vs = vs = std::string(vertex);
    program->fs = fs = std::string(fragment);
  }
}

void Shader::set_uniform(const char *name, float32 f)
{
  GLint location = program->get_uniform_location(name);
  check_err(location, name);
  glUniform1fv(location, 1, &f);
}

void Shader::set_uniform(const char *name, uint32 i)
{
  GLint location = program->get_uniform_location(name);
  check_err(location, name);
  glUniform1ui(location, i);
}
void Shader::set_uniform(const char *name, int32 i)
{
  GLint location = program->get_uniform_location(name);
  check_err(location, name);
  glUniform1i(location, i);
}

void Shader::set_uniform(const char *name, const glm::vec2& v)
{
  GLint location = program->get_uniform_location(name);
  check_err(location, name);
  glUniform2fv(location, 1, &v[0]);
}

void Shader::set_uniform(const char *name, const glm::vec3 &v)
{
  GLint location = program->get_uniform_location(name);
  check_err(location, name);
  glUniform3fv(location, 1, &v[0]);
}
void Shader::set_uniform(const char *name, const glm::vec4 &v)
{
  GLint location = program->get_uniform_location(name);
  check_err(location, name);
  glUniform4fv(location, 1, &v[0]);
}
void Shader::set_uniform(const char *name, const glm::mat4 &m)
{
  GLint location = program->get_uniform_location(name);
  check_err(location, name);
  glUniformMatrix4fv(location, 1, GL_FALSE, &m[0][0]);
}
GLint Shader_Handle::get_uniform_location(const char *name)
{
  GLint location;
  auto search = location_cache.find(name);
  if (search == location_cache.end())
  {
    location = glGetUniformLocation(program, name);
    location_cache[name] = location;
  }
  else
  {
    location = search->second;
  }
  return location;
}
void Shader::use() const { glUseProgram(program->program); }

static double get_time()
{
  static const Uint64 freq = SDL_GetPerformanceFrequency();
  static const Uint64 begin_time = SDL_GetPerformanceCounter();

  Uint64 current = SDL_GetPerformanceCounter();
  Uint64 elapsed = current - begin_time;
  return (float64)elapsed / (float64)freq;
}
void Shader::check_err(GLint loc, const char *name)
{
  if (loc == -1)
  {
    // set_message("Shader invalid uniform: ", name);
  }
}

void Shader_Handle::set_location_cache()
{
#ifdef DEBUG
  GLint id;
  glGetIntegerv(GL_CURRENT_PROGRAM, &id);
  ASSERT(id == program);
#endif
  // set_message(s("Shader: ", vs, fs));
  // set_message("Assigning texture units to shader samplers");
  std::string str;
  for (uint32 i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
  {
    str = s("texture", i);
    GLint locationc = get_uniform_location(str.c_str());
    GLint location = glGetUniformLocation(program, str.c_str());
    ASSERT(location == locationc); /*
     set_message(s("Assigning sampler uniform name: ", str,
         " with location: ", location, " to texture unit: ", i));*/
    glUniform1i(location, i);
  }
  for (uint32 i = 0; i < MAX_LIGHTS; ++i)
  {
    str = s("shadow_maps[", i, "]");
    GLint locationc = get_uniform_location(str.c_str());

    GLint location = glGetUniformLocation(program, str.c_str());

    ASSERT(location == locationc);

    // set_message(s("Assigning sampler uniform name: ", str, " with location: ",
    //    location, " to texture unit: ", (GLuint)Texture_Location::s0 + i));
    glUniform1i(location, (GLuint)Texture_Location::s0 + i);
  }

  if (light_location_cache_set)
    return;

  for (int i = 0; i < MAX_LIGHTS; ++i)
  {
    str = s("lights[", i, "].position");
    light_locations_cache[i].position = glGetUniformLocation(program, str.c_str());

    str = s("lights[", i, "].direction");
    light_locations_cache[i].direction = glGetUniformLocation(program, str.c_str());

    str = s("lights[", i, "].flux");
    light_locations_cache[i].flux = glGetUniformLocation(program, str.c_str());

    str = s("lights[", i, "].attenuation");
    light_locations_cache[i].attenuation = glGetUniformLocation(program, str.c_str());

    str = s("lights[", i, "].ambient");
    light_locations_cache[i].ambient = glGetUniformLocation(program, str.c_str());

    str = s("lights[", i, "].cone_angle");
    light_locations_cache[i].cone_angle = glGetUniformLocation(program, str.c_str());

    str = s("lights[", i, "].type");
    light_locations_cache[i].type = glGetUniformLocation(program, str.c_str());

    str = s("shadow_map_transform[", i, "]");
    light_locations_cache[i].shadow_map_transform = glGetUniformLocation(program, str.c_str());

    str = s("max_variance[", i, "]");
    light_locations_cache[i].max_variance = glGetUniformLocation(program, str.c_str());

    str = s("shadow_map_enabled[", i, "]");
    light_locations_cache[i].shadow_map_enabled = glGetUniformLocation(program, str.c_str());
  }

  light_count_location = glGetUniformLocation(program, "number_of_lights");
  light_location_cache_set = true;
}