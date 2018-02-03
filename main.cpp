#include "Globals.h"
#include "Render.h"
#include "State.h"
#include "Warg_State.h"
#include "Render_Test_State.h"
#include "Timer.h"
#include <SDL2/SDL.h>
#undef main
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



int main(int argc, char *argv[])
{
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
  set_message("OpenGL Version.",
              std::to_string(major) + " " + std::to_string(minor));
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
  glbinding::setCallbackMaskExcept(glbinding::CallbackMask::After |
  glbinding::CallbackMask::ParametersAndReturnValue, { "glGetError", "glFlush" });
#if ENABLE_OPENGL_ERROR_CATCHING_AND_LOG
  glbinding::setBeforeCallback(gl_before_check);
  glbinding::setAfterCallback(gl_after_check);
#endif
  glClearColor(0, 0, 0, 1);
  checkSDLError(__LINE__);

  SDL_ClearError();
  float64 last_time = 0.0;
  float64 elapsed_time = 0.0;
  INIT_RENDERER();

  std::vector<State *> states;
  Warg_State game_state("Game State", window, window_size);
  states.push_back((State *)&game_state);
  Render_Test_State render_test_state("Render Test State", window, window_size);
  states.push_back((State *)&render_test_state);
  State *current_state = &*states[0];
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

    while (current_state->current_time + dt < last_time + elapsed_time)
    {
      State *s = current_state;
      s->current_time += dt;
      current_state->handle_input(&current_state, states);
      s->update();
      if (s != current_state)
      {
        s->paused = true;
        current_state->renderer.set_render_scale(
            current_state->renderer.get_render_scale());
        break;
      }
    }
    current_state->render(current_state->current_time);
    current_state->performance_output();
  }
  push_log_to_disk();
  CLEANUP_RENDERER();
  SDL_Quit();
  return 0;
}
