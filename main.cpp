#include "Globals.h"
#include "Render.h"
#include "Render_Test_State.h"
#include "State.h"
#include "Timer.h"
#include "Warg_State.h"
#include <SDL2/SDL.h>
#undef main
#include "Third_party/imgui/imgui.h"
#include "Third_party/imgui/imgui_internal.h"
#include "SDL_Imgui_State.h"
#include <enet/enet.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>

#include <glbinding/Binding.h>

void gl_before_check(const glbinding::FunctionCall &f)
{
  std::string opengl_call = f.function->name();
  opengl_call += '(';
  for (size_t i = 0; i < f.parameters.size(); ++i)
  {
    opengl_call += f.parameters[i]->asString();
    if (i < f.parameters.size() - 1)
      opengl_call += ", ";
  }
  opengl_call += ")";

  if (f.returnValue)
    opengl_call += " Returned: " + f.returnValue->asString();

  set_message("BEFORE OPENGL call: ", opengl_call);
  check_gl_error();
}
void gl_after_check(const glbinding::FunctionCall &f)
{
  std::string opengl_call = f.function->name();
  opengl_call += '(';
  for (size_t i = 0; i < f.parameters.size(); ++i)
  {
    opengl_call += f.parameters[i]->asString();
    if (i < f.parameters.size() - 1)
      opengl_call += ", ";
  }
  opengl_call += ")";

  if (f.returnValue)
    opengl_call += " Returned: " + f.returnValue->asString();

  set_message("", opengl_call);
  check_gl_error();
}

void server_main()
{
  ASSERT(!enet_initialize());

  auto warg_server = std::make_unique<Warg_Server>(false);

  float64 current_time = get_real_time();
  float64 last_time = 0.0;
  float64 elapsed_time = 0.0;
  float64 dt = 1.0 / 30.0;
  while (true)
  {
    const float64 time = get_real_time();
    elapsed_time = time - current_time;
    last_time = current_time;
    while (current_time + dt < last_time + elapsed_time)
    {
      current_time += dt;
      warg_server->update(dt);
    }
  }

  // enet_host_destroy(warg_server->server());
  enet_deinitialize();
}

int main(int argc, char *argv[])
{
  bool client = false;
  std::string address;
  std::string char_name;
  uint8_t team;

  WARG_SERVER = false;
  if (argc > 1 && std::string(argv[1]) == "--server")
  {
    WARG_SERVER = true;
	ASSERT(WARG_SERVER);
    server_main();
    return 0;
  }
  else if (argc > 1 && std::string(argv[1]) == "--connect")
  {
    client = true;

    if (argc <= 4)
    {
      std::cout << "Please provide arguments in format: --connect IP_ADDRESS "
                   "CHARACTER_NAME CHARACTER_TEAM\nFor example: warg --connect "
                   "127.0.0.1 Warriorguy 0"
                << std::endl;
      return 1;
    }
    address = argv[2];
    char_name = argv[3];
    team = std::stoi(argv[4]);
  }
  ASSERT(!WARG_SERVER);

  SDL_ClearError();
  generator.seed(1234);
  SDL_Init(SDL_INIT_EVERYTHING);
  uint32 display_count = uint32(SDL_GetNumVideoDisplays());
  std::stringstream s;
  for (uint32 i = 0; i < display_count; ++i)
  {
    s << "Display " << i << ":\n";
    SDL_DisplayMode mode;
    uint32 mode_count = uint32(SDL_GetNumDisplayModes(i));
    for (uint32 j = 0; j < mode_count; ++j)
    {
      SDL_GetDisplayMode(i, j, &mode);
      s << "Supported resolution: " << mode.w << "x" << mode.h << " "
        << mode.refresh_rate << "hz  " << SDL_GetPixelFormatName(mode.format)
        << "\n";
    }
  }
  set_message(s.str());

  ivec2 window_size = {1280, 720};
  int32 flags = SDL_WINDOW_OPENGL;
  // SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

  SDL_Window *window =
      SDL_CreateWindow("title", 100, 130, window_size.x, window_size.y, flags);

  SDL_GLContext context = SDL_GL_CreateContext(window);

  int32 major, minor;
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
  set_message(
      "OpenGL Version.", std::to_string(major) + " " + std::to_string(minor));
  if (major <= 3)
  {
    if (major < 3 || minor < 1)
    {
      set_message("Unsupported OpenGL Version.");
      push_log_to_disk();
      throw;
    }
  }
  // 1 vsync, 0 no vsync, -1 late-swap
  int32 swap = SDL_GL_SetSwapInterval(0);
  if (swap == -1)
  {
    swap = SDL_GL_SetSwapInterval(1);
  }

  glbinding::Binding::initialize();
  glbinding::setCallbackMaskExcept(
      glbinding::CallbackMask::After |
          glbinding::CallbackMask::ParametersAndReturnValue,
      {"glGetError", "glFlush"});
#if ENABLE_OPENGL_ERROR_CATCHING_AND_LOG
  glbinding::setBeforeCallback(gl_before_check);
  glbinding::setAfterCallback(gl_after_check);
#endif
  glClearColor(0, 0, 0, 1);
  checkSDLError(__LINE__);

  SDL_ClearError();

  SDL_Imgui_State state_imgui(window);
  SDL_Imgui_State renderer_imgui(window);
  float64 last_time = 0.0;
  float64 elapsed_time = 0.0;

  Warg_State *game_state;
  if (client)
    game_state = new Warg_State(
        "Warg", window, window_size, address.c_str(), char_name.c_str(), team);
  else
    game_state = new Warg_State("Warg", window, window_size);
  std::vector<State *> states;
  states.push_back((State *)game_state);
  Render_Test_State render_test_state("Render Test State", window, window_size);
  states.push_back((State *)&render_test_state);
  State *current_state = &*states[0];
  std::vector<SDL_Event> input_events;

  // todo: compositor for n state/renderer pairs with ui

  SDL_SetRelativeMouseMode(SDL_bool(false));
  while (current_state->running)
  {
    const float64 real_time = get_real_time();
    if (current_state->paused)
    {
      float64 past_accum = current_state->paused_time_accumulator;
      float64 real_time_of_last_update =
          current_state->current_time + past_accum;
      float64 real_time_since_last_update =
          real_time - real_time_of_last_update;
      current_state->paused_time_accumulator += real_time_since_last_update;
      current_state->paused = false;
      continue;
    }
    const float64 time = real_time - current_state->paused_time_accumulator;
    elapsed_time = time - current_state->current_time;
    set_message("time", std::to_string(time), 1);
    if (elapsed_time > 0.3)
      elapsed_time = 0.3;
    last_time = current_state->current_time;

    state_imgui.bind();

    bool draw_state_ui = true;
    bool draw_render_ui = true;

    state_imgui.bind();
    while (current_state->current_time + dt < last_time + elapsed_time)
    {
      State *s = current_state;
      s->current_time += dt;
      state_imgui.handle_input();

      bool block_kb = state_imgui.context->IO.WantTextInput |
                      renderer_imgui.context->IO.WantTextInput;
      bool block_mouse = state_imgui.context->IO.WantCaptureMouse |
                         renderer_imgui.context->IO.WantCaptureMouse;

      current_state->handle_input(&current_state, &states,
          state_imgui.event_output, block_kb, block_mouse);

      for (auto &e : state_imgui.event_output)
      {
        input_events.push_back(e);
      }
      state_imgui.event_output.clear();
      state_imgui.new_frame(window);
      s->update();
      state_imgui.end_frame();
      bool last_state_update =
          !(current_state->current_time + dt < last_time + elapsed_time);

      if (s != current_state)
      {
        last_state_update = true;
        s->paused = true;
        current_state->renderer.set_render_scale(
            current_state->renderer.get_render_scale());
      }
      if (last_state_update)
      {
        if (draw_state_ui)
          state_imgui.build_draw_data();
        break;
      }
    }

    renderer_imgui.bind();
    renderer_imgui.new_frame(window);

    for (auto &e : input_events)
    {
      SDL_PushEvent(&e);
    }
    input_events.clear();
    renderer_imgui.handle_input();
    renderer_imgui.event_output.clear();

    current_state->render(current_state->current_time);

    renderer_imgui.end_frame();

    if (draw_state_ui)
    {
      state_imgui.bind();
      state_imgui.render();
    }
    if (draw_render_ui)
    {
      renderer_imgui.bind();
      renderer_imgui.build_draw_data();
      renderer_imgui.render();
    }

    SDL_GL_SwapWindow(window);
    set_message("FRAME END", "");
    SWAP_TIMER.stop();
    FRAME_TIMER.start();

    current_state->performance_output();
  }
  delete game_state;
  push_log_to_disk();
  state_imgui.destroy();
  renderer_imgui.destroy();
  SDL_Quit();
  return 0;
}
