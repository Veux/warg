#pragma once
#include "Render.h"
#include "Third_Party/imgui/imgui.h"

struct SDL_Window;
typedef union SDL_Event SDL_Event;

struct SDL_Imgui_State
{

  SDL_Imgui_State(SDL_Window *window);
  void destroy();

  void bind();
  void new_frame(SDL_Window *window);
  void end_frame();

  void handle_input();
  void build_draw_data();
  void render();

  ImGuiIO *state_io = nullptr;
  ImGuiContext *context = nullptr;

  std::vector<SDL_Event> event_output;

private:
  bool process_event(SDL_Event *event);
  bool init(SDL_Window *window);
  static const char *get_clipboard(void *);
  static void set_clipboard(void *, const char *text);
  void create_fonts_texture();
  void invalidate_device_objects();
  bool create_device_objects();

  ImDrawData *draw_data = nullptr;

  Uint64 time = 0;
  bool mouse_pressed[3] = {false, false, false};
  GLuint font_texture = 0;
  int shader_handle = 0, vert_handle = 0, frag_handle = 0;
  int texture_location = 0, projection_location = 0;
  int position_location = 0, uv_location = 0, color_location = 0;
  unsigned int vbo = 0, vao = 0, element_buffer = 0;
  SDL_Cursor *sdl_cursors[ImGuiMouseCursor_Count_] = {0};
};