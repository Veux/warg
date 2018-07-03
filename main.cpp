
#include "Json.h"

#include "Globals.h"
#include "Render.h"
#include "Render_Test_State.h"
#include "State.h"
#include "Timer.h"
#include "Warg_State.h"
#include <SDL2/SDL.h>
#undef main
#include "SDL_Imgui_State.h"
#include "Third_party/imgui/imgui.h"
#include "Third_party/imgui/imgui_internal.h"
#include <enet/enet.h>
#include <glbinding/Binding.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>

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
  const char *config_filename = "config.json";
  CONFIG.load(config_filename);
  SDL_Delay(1000);
  bool client = false;
  std::string address;
  std::string char_name;
  uint8_t team;

  WARG_SERVER = false;
  IMAGE_LOADER.init();

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
  generator.seed(uint32(SDL_GetPerformanceCounter()));
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
      s << "Supported resolution: " << mode.w << "x" << mode.h << " " << mode.refresh_rate << "hz  "
        << SDL_GetPixelFormatName(mode.format) << "\n";
    }
  }
  set_message(s.str());

  ivec2 window_size = {CONFIG.resolution.x, CONFIG.resolution.y};
  int32 flags = SDL_WINDOW_OPENGL;
  // SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

  SDL_Window *window = SDL_CreateWindow("title", 100, 130, window_size.x, window_size.y, flags);

  SDL_GLContext context = SDL_GL_CreateContext(window);

  int32 major, minor;
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
  set_message("OpenGL Version.", std::to_string(major) + " " + std::to_string(minor));
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
      glbinding::CallbackMask::After | glbinding::CallbackMask::ParametersAndReturnValue, {"glGetError", "glFlush"});
#if ENABLE_OPENGL_ERROR_CATCHING_AND_LOG
  glbinding::setBeforeCallback(gl_before_check);
  glbinding::setAfterCallback(gl_after_check);
#endif
  glClearColor(0, 0, 0, 1);
  checkSDLError(__LINE__);

  SDL_ClearError();

  SDL_Imgui_State trash_imgui(window);
  SDL_Imgui_State imgui(window);
  ImGui::StyleColorsDark();
  check_gl_error();
  trash_imgui.bind();
  trash_imgui.new_frame(window, 0.1f);
  trash_imgui.end_frame();

  float64 last_time = 0.0;
  float64 elapsed_time = 0.0;
  Render_Test_State render_test_state("Render Test State", window, window_size);

  Warg_State *game_state;
  if (client)
    game_state = new Warg_State("Warg", window, window_size, address.c_str(), char_name.c_str(), team);
  else
    game_state = new Warg_State("Warg", window, window_size);
  std::vector<State *> states;
  states.push_back((State *)game_state);
  states.push_back((State *)&render_test_state);
  State *current_state = &*states[0];
  std::vector<SDL_Event> imgui_event_accumulator;

  bool first_update = false;
  // todo: compositor for n state/renderer pairs with ui

  SDL_SetRelativeMouseMode(SDL_bool(false));
  bool renderer_requires_trashgui_wrapping = false;
  while (current_state->running)
  {
    const float64 real_time = get_real_time();
    if (current_state->paused)
    {
      float64 past_accum = current_state->paused_time_accumulator;
      float64 real_time_of_last_update = current_state->current_time + past_accum;
      float64 real_time_since_last_update = real_time - real_time_of_last_update;
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

    trash_imgui.bind();
    renderer_requires_trashgui_wrapping = true;
    float64 imgui_dt_accumulator = 0;
    while (current_state->current_time + dt < last_time + elapsed_time)
    {
      first_update = true;
      State *state_ptr = current_state;
      state_ptr->current_time += dt;
      imgui_dt_accumulator += dt;
      bool last_state_update = !(state_ptr->current_time + dt < last_time + elapsed_time);

      std::vector<SDL_Event> new_events;
      SDL_Event e;
      while (SDL_PollEvent(&e))
      {
        new_events.push_back(e);
        imgui_event_accumulator.push_back(e);
      }

      imgui.ignore_all_input = state_ptr->free_cam;

      state_ptr->handle_input(
          &current_state, &states, &new_events, imgui.context->IO.WantTextInput, imgui.context->IO.WantCaptureMouse);

      if (state_ptr != current_state)
      {
        state_ptr->paused = true;
        current_state->renderer.set_render_scale(state_ptr->renderer.get_render_scale());
        break;
      }

      if (last_state_update)
      { // possible bug: two clicks faster than 1 frame aren't seen

        imgui.bind();
        imgui.handle_input(&imgui_event_accumulator);
        imgui_event_accumulator.clear();
        imgui.new_frame(window, imgui_dt_accumulator);
        state_ptr->update();
        renderer_requires_trashgui_wrapping = false;
      }
      else
      {
        trash_imgui.bind();
        trash_imgui.new_frame(window, dt);
        state_ptr->update();
        trash_imgui.end_frame();
      }
    }

    if (!first_update)
      continue;

    if (renderer_requires_trashgui_wrapping)
    {
      ASSERT(ImGui::GetCurrentContext() == trash_imgui.context);
      trash_imgui.new_frame(window, 0.1f);
    }
    current_state->render(current_state->current_time);

    std::string messages = get_messages();
    ImGui::Text(messages.c_str());

    static bool show_demo_window = true;
    if (show_demo_window)
    {
      ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
      ImGui::ShowDemoWindow(&show_demo_window);
    }
    if (renderer_requires_trashgui_wrapping)
    {
      ASSERT(ImGui::GetCurrentContext() == trash_imgui.context);
      trash_imgui.end_frame();
    }
    if (ImGui::GetCurrentContext() == imgui.context)
    {
      imgui.build_draw_data();
      imgui.render();
      imgui.end_frame();
    }
    else
    {
      imgui.bind();
      imgui.render();
      trash_imgui.bind();
    }

    SDL_GL_SwapWindow(window);
    set_message("FRAME END", "");
    SWAP_TIMER.stop();
    FRAME_TIMER.start();

    current_state->performance_output();
  }
  delete game_state;
  push_log_to_disk();
  imgui.destroy();
  SDL_Quit();
  CONFIG.save(config_filename);
  return 0;
}
