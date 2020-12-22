#pragma once
#include "Third_Party/imgui/imgui.h"
#include "Third_party/imgui/imgui_internal.h"
#include <glm/glm.hpp>
#include <glad/glad.h>

struct Texture;
struct Texture_Descriptor;
struct Texture_Handle;
struct SDL_Window;
typedef union SDL_Event SDL_Event;

struct Imgui_Texture_Descriptor
{
  std::shared_ptr<Texture_Handle> ptr = nullptr;
  glm::vec2 size = glm::vec2(0);
  float mip_lod_to_draw = 0.f;
  bool y_invert = false;
  GLenum internalformat = 0;
  bool is_mipmap_list_command = false;
};

struct SDL_Imgui_State
{
  SDL_Imgui_State();
  void init(SDL_Window *window);
  void destroy();

  void new_frame(SDL_Window *window, glm::float64 dt_since_last_frame);
  void end_frame();

  void handle_input(std::vector<SDL_Event> *input);
  bool ignore_all_input = false;
  bool draw_cursor = true;

  void build_draw_data();
  void render();

  ImGuiContext *context = nullptr;
  ImGuiIO *state_io = nullptr;

  glm::ivec2 cursor_position = glm::ivec2(0);
  glm::uint32 mouse_state = 0;
  
  GLuint font_texture = 0;
  void invalidate_device_objects();
private:
  bool process_event(SDL_Event *event);
  static const char *get_clipboard(void *);
  static void set_clipboard(void *, const char *text);
  void create_fonts_texture();
  bool create_device_objects();

  ImDrawData *draw_data = nullptr;

  bool mouse_pressed[3] = {false, false, false};
  int shader_handle = 0, vert_handle = 0, frag_handle = 0;
  int texture_location = 0, projection_location = 0;
  int position_location = 0, uv_location = 0, color_location = 0;
  int gamma_location = 0;
  int mip_location = 0;
  int sample_lod_location = 0;
  GLuint vbo = 0, vao = 0, element_buffer = 0;
  SDL_Cursor *sdl_cursors[ImGuiMouseCursor_Count_] = {0};
};

void put_imgui_texture(Texture *t, glm::vec2 size, bool y_invert = false);
void put_imgui_texture(Texture_Descriptor *td, glm::vec2 size, bool y_invert = false);
void put_imgui_texture(std::shared_ptr<Texture_Handle> t, glm::vec2 size, bool y_invert = false);
void put_imgui_texture(Imgui_Texture_Descriptor *itd);

bool put_imgui_texture_button(Texture *t, glm::vec2 size, bool y_invert = false);
bool put_imgui_texture_button(Texture_Descriptor *td, glm::vec2 size, bool y_invert = false);
bool put_imgui_texture_button(std::shared_ptr<Texture_Handle> t, glm::vec2 size, bool y_invert = false);
bool put_imgui_texture_button(Imgui_Texture_Descriptor *itd);