#include "stdafx.h"
#include "Forward_Declarations.h"
#include "Globals.h"
#include "Json.h"
#include "Render.h"
#include "Render_Test_State.h"
#include "State.h"
#include "Timer.h"
#include "Warg_State.h"
#include "SDL_Imgui_State.h"

#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/misc/freetype/imgui_freetype.cpp>

struct FreeTypeTest
{
  enum FontBuildMode
  {
    FontBuildMode_FreeType,
    FontBuildMode_Stb
  };

  FontBuildMode BuildMode;
  bool WantRebuild;
  float FontsMultiply;
  int FontsPadding;
  unsigned int FontsFlags;

  FreeTypeTest()
  {
    BuildMode = FontBuildMode_FreeType;
    WantRebuild = true;
    FontsMultiply = 1.0f;
    FontsPadding = 1;
    FontsFlags = 0;
  }

  // Call _BEFORE_ NewFrame()
  bool UpdateRebuild()
  {
    if (!WantRebuild)
      return false;
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->TexGlyphPadding = FontsPadding;
    for (int n = 0; n < io.Fonts->ConfigData.Size; n++)
    {
      ImFontConfig *font_config = (ImFontConfig *)&io.Fonts->ConfigData[n];
      font_config->RasterizerMultiply = FontsMultiply;
      font_config->RasterizerFlags = (BuildMode == FontBuildMode_FreeType) ? FontsFlags : 0x00;
    }
    if (BuildMode == FontBuildMode_FreeType)
      ImGuiFreeType::BuildFontAtlas(io.Fonts, FontsFlags);
    else if (BuildMode == FontBuildMode_Stb)
      io.Fonts->Build();
    WantRebuild = false;
    return true;
  }

  // Call to draw interface
  void ShowFreetypeOptionsWindow()
  {
    ImGui::Begin("FreeType Options");
    ImGui::ShowFontSelector("Fonts");
    WantRebuild |= ImGui::RadioButton("FreeType", (int *)&BuildMode, FontBuildMode_FreeType);
    ImGui::SameLine();
    WantRebuild |= ImGui::RadioButton("Stb (Default)", (int *)&BuildMode, FontBuildMode_Stb);
    WantRebuild |= ImGui::DragFloat("Multiply", &FontsMultiply, 0.001f, 0.0f, 2.0f);
    WantRebuild |= ImGui::DragInt("Padding", &FontsPadding, 0.1f, 0, 16);
    if (BuildMode == FontBuildMode_FreeType)
    {
      WantRebuild |= ImGui::CheckboxFlags("NoHinting", &FontsFlags, ImGuiFreeType::NoHinting);
      WantRebuild |= ImGui::CheckboxFlags("NoAutoHint", &FontsFlags, ImGuiFreeType::NoAutoHint);
      WantRebuild |= ImGui::CheckboxFlags("ForceAutoHint", &FontsFlags, ImGuiFreeType::ForceAutoHint);
      WantRebuild |= ImGui::CheckboxFlags("LightHinting", &FontsFlags, ImGuiFreeType::LightHinting);
      WantRebuild |= ImGui::CheckboxFlags("MonoHinting", &FontsFlags, ImGuiFreeType::MonoHinting);
      WantRebuild |= ImGui::CheckboxFlags("Bold", &FontsFlags, ImGuiFreeType::Bold);
      WantRebuild |= ImGui::CheckboxFlags("Oblique", &FontsFlags, ImGuiFreeType::Oblique);
      WantRebuild |= ImGui::CheckboxFlags("Monochrome", &FontsFlags, ImGuiFreeType::Monochrome);
    }
    ImGui::End();
  }
};

void glad_callback(const char *name, void *funcptr, int len_args, ...)
{
  if (!WARG_RUNNING)
  {
    return;
  }
  // return;
  GLenum error_code;
  error_code = glad_glGetError();
  // check_gl_error();
  va_list arg;
  va_start(arg, len_args);
  for (uint32 i = 0; i < len_args; ++i)
  {
  }

  if (error_code != GL_NO_ERROR)
  {
    std::string error;
    switch (error_code)
    {
      case GL_INVALID_OPERATION:
        error = "INVALID_OPERATION";
        break;
      case GL_INVALID_ENUM:
        error = "INVALID_ENUM";
        break;
      case GL_INVALID_VALUE:
        error = "INVALID_VALUE";
        break;
      case GL_OUT_OF_MEMORY:
        error = "OUT_OF_MEMORY";
        break;
      case GL_INVALID_FRAMEBUFFER_OPERATION:
        error = "INVALID_FRAMEBUFFER_OPERATION";
        break;
    }
    set_message("GL ERROR", "GL_" + error);
    push_log_to_disk();
    ASSERT(0);
  }
}

void input_preprocess(SDL_Event &e, State **current_state, std::vector<State *> &available_states, bool WantTextInput,
    bool WantCaptureMouse, std::vector<SDL_Event> *imgui_events, std::vector<SDL_Event> *state_key_events,
    std::vector<SDL_Event> *state_mouse_events)
{
  if (e.type == SDL_QUIT)
  {
    WARG_RUNNING = false;
    return;
  }
  if (e.type == SDL_KEYDOWN)
  {
    if (e.key.keysym.sym == SDLK_ESCAPE)
    {
      WARG_RUNNING = false;
      return;
    }
  }
  if (e.type == SDL_WINDOWEVENT)
  {
    if (e.window.event == SDL_WINDOWEVENT_RESIZED)
    {
      // resize
      return;
    }
    else if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED || e.window.event == SDL_WINDOWEVENT_ENTER)
    { // dumping mouse delta prevents camera teleport on focus gain

      for (State *&state : available_states)
      {
        SDL_GetMouseState(&state->cursor_position.x, &state->cursor_position.y);
      }
      return;
    }
  }

  if (e.type == SDL_KEYUP)
  {
    if (e.key.keysym.sym == SDLK_F1)
    {
      if (available_states.size() < 1)
        return;

      (*current_state)->recieves_input = false;
      *current_state = available_states[0];
      (*current_state)->recieves_input = true;

      ivec2 trash;
      SDL_GetRelativeMouseState(&trash.x, &trash.y);
      //(*current_state)->reset_mouse_delta();
      (*current_state)->renderer.previous_color_target_missing = true;
      return;
    }
    else if (e.key.keysym.sym == SDLK_F2)
    {
      if (available_states.size() < 2)
        return;
      ;
      (*current_state)->recieves_input = false;
      *current_state = available_states[1];
      (*current_state)->recieves_input = true;

      if ((*current_state)->free_cam)
        SDL_SetRelativeMouseMode(SDL_bool(true));
      else
      {
        SDL_SetRelativeMouseMode(SDL_bool(false));
        SDL_WarpMouseInWindow(nullptr, (*current_state)->cursor_position.x, (*current_state)->cursor_position.y);
      }
      ivec2 trash;
      SDL_GetRelativeMouseState(&trash.x, &trash.y);
      //(*current_state)->reset_mouse_delta();
      (*current_state)->renderer.previous_color_target_missing = true;
      return;
    }
    else if (e.key.keysym.sym == SDLK_F3)
    {
      if (available_states.size() < 3)
        return;
      ;
      (*current_state)->recieves_input = false;
      *current_state = available_states[2];
      (*current_state)->recieves_input = true;

      ivec2 trash;
      SDL_GetRelativeMouseState(&trash.x, &trash.y);
      //(*current_state)->reset_mouse_delta();
      (*current_state)->renderer.previous_color_target_missing = true;
      return;
    }
    else if (e.key.keysym.sym == SDLK_F4)
    {
      if (available_states.size() < 4)
        return;
      ;
      (*current_state)->recieves_input = false;
      *current_state = available_states[3];
      (*current_state)->recieves_input = true;

      ivec2 trash;
      SDL_GetRelativeMouseState(&trash.x, &trash.y);
      (*current_state)->renderer.previous_color_target_missing = true;
      return;
    }
  }

  if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP || e.type == SDL_TEXTINPUT)
  {
    if (WantTextInput && (!IMGUI.ignore_all_input))
    {
      imgui_events->push_back(e);
      return;
    }
    else
    {
      if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)
        state_key_events->push_back(e);
      return;
    }
  }

  if (e.type ==
      SDL_MOUSEWHEEL) // e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEWHEEL)
  {
    if (WantCaptureMouse && (!IMGUI.ignore_all_input))
    {
      imgui_events->push_back(e);
      return;
    }
    else
    {
      state_mouse_events->push_back(e);
      return;
    }
  }

  return;
}

int main(int argc, char *argv[])
{
  MAIN_THREAD_ID = std::this_thread::get_id();
  const char *config_filename = "config.json";
  CONFIG.load(config_filename);
  // SDL_Delay(1000); ??
  bool client = false;
  std::string address;
  std::string char_name;
  uint8_t team;
  int port = 8765;

  WARG_SERVER = false;
  ASSERT(!enet_initialize());
  atexit(enet_deinitialize);
  if (argc > 1 && std::string(argv[1]) == "--server")
  {
    WARG_SERVER = true;
    ASSERT(WARG_SERVER);
    Warg_Server *server = new Warg_Server();
    server->run(port);
    return 0;
  }
  else if (argc > 1 && std::string(argv[1]) == "--connect")
  {
    std::cout << "CONNECTING" << std::endl;
    client = true;

    if (argc <= 4)
    {
      std::cout << "Please provide arguments in format: --connect IP_ADDRESS "
                   "CHARACTER_NAME CHARACTER_TEAM\nFor example: warg --connect "
                   "127.0.0.1 Cubeboi 0"
                << std::endl;
      return 1;
    }
    address = argv[2];
    char_name = argv[3];
    team = std::stoi(argv[4]);
  }

  ASSERT(!WARG_SERVER);
  generator.seed(uint32(SDL_GetPerformanceCounter()));

  SDL_GLContext context = nullptr;
  SDL_Window *window = nullptr;
  ivec2 window_size = ivec2(0);
  if (PROCESS_USES_SDL_AND_OPENGL)
  {
    SDL_ClearError();
    SDL_Init(SDL_INIT_EVERYTHING);
    uint32 display_count = uint32(SDL_GetNumVideoDisplays());
    std::stringstream ss;
    for (uint32 i = 0; i < display_count; ++i)
    {
      ss << "Display " << i << ":\n";
      SDL_DisplayMode mode;
      uint32 mode_count = uint32(SDL_GetNumDisplayModes(i));
      for (uint32 j = 0; j < mode_count; ++j)
      {
        SDL_GetDisplayMode(i, j, &mode);
        ss << "Supported resolution: " << mode.w << "x" << mode.h << " " << mode.refresh_rate << "hz  "
           << SDL_GetPixelFormatName(mode.format) << "\n";
      }
    }
    set_message(ss.str());

    window_size = {CONFIG.resolution.x, CONFIG.resolution.y};
    int32 flags = SDL_WINDOW_OPENGL;
    // SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    // SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    window = SDL_CreateWindow("Warg_Engine", 100, 130, window_size.x, window_size.y, flags);
    SDL_RaiseWindow(window);
    context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    int32 major, minor;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
    set_message("OpenGL Version.", std::to_string(major) + " " + std::to_string(minor));
    if (major <= 4)
    {
      if (major < 4 || minor < 5)
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
    gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);

    // set_message("glad OpenGL", s(GLVersion.major, ".", GLVersion.minor));
    // GLAPI void glad_set_pre_callback(gl_before_check);

    // glbinding::Binding::initialize();
    // glbinding::setCallbackMaskExcept(
    //   glbinding::CallbackMask::After | glbinding::CallbackMask::ParametersAndReturnValue, {"glGetError",
    //   "glFlush"});
#if ENABLE_OPENGL_ERROR_CATCHING_AND_LOG

    // glbinding::setBeforeCallback(gl_before_check);
    // glbinding::setAfterCallback(gl_after_check);
#endif

    glClearColor(0, 0, 0, 1);
    checkSDLError(__LINE__);
    SDL_ClearError();
    SDL_SetRelativeMouseMode(SDL_bool(false));
  }
  // glad_set_pre_callback(glad_callback);
  // glad_set_post_callback(glad_callback);
  // Local_Session warg_session = Local_Session();

  Session *warg_session = nullptr;
  if (client)
  {
    std::cout << "CLIENT TRUE" << std::endl;
    Network_Session *network_session = new Network_Session();
    network_session->connect(address.c_str(), port);
    warg_session = (Session *)network_session;
    std::cout << "connected successfully" << std::endl;
  }
  else
  {
    warg_session = (Session *)new Local_Session();
    char_name = "Cubeboi";
    team = 0;
  }

  IMGUI.init(window);
  ImGui::SetCurrentContext(IMGUI.context);
  ImGui::StyleColorsDark();
  ImGuiIO io = ImGui::GetIO();
  // io.Fonts->AddFontFromFileTTF("Assets/Fonts/LiberationSans-Regular.ttf", 16.0f);
  // io.Fonts->AddFontFromFileTTF("Assets/Fonts/LiberationSans-Regular.ttf", 14.0f);
  // io.Fonts->AddFontFromFileTTF("Assets/Fonts/Roboto-Medium.ttf", 14.0f);
  // io.Fonts->AddFontFromFileTTF("Assets/Fonts/FrizQuadrataTT.ttf", 14.0f);
  // io.Fonts->AddFontFromFileTTF("Assets/Fonts/Roboto-Medium.ttf", 16.0f);
  // io.Fonts->AddFontFromFileTTF("Assets/Fonts/FrizQuadrataTT.ttf", 16.0f);
  // io.Fonts->AddFontFromFileTTF("Assets/Fonts/FrizQuadrataTT.ttf", 18.0f);
  // io.Fonts->AddFontFromFileTTF("Assets/Fonts/FrizQuadrataTT.ttf", 18.0f);

  // io.Fonts->AddFontFromFileTTF("Assets/Fonts/spleen-8x16.psfu", 18.0f);
  ImFontConfig font_cfg;
  font_cfg.OversampleH = 1;
  font_cfg.OversampleV = 1;
  font_cfg.PixelSnapH = true;
  font_cfg.RasterizerFlags = ImGuiFreeType::Monochrome | ImGuiFreeType::MonoHinting;

  io.Fonts->AddFontFromFileTTF("Assets/Fonts/spleen-8x16.otf", 16.0f, &font_cfg);

  // save texture atlas to file:
  //  IMGUI.new_frame(window, 1);
  //  GLuint font_texture = IMGUI.font_texture;
  //
  //  GLint w,h;
  //
  //  glBindTexture(GL_TEXTURE_2D,font_texture);
  //  glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&w);
  //  glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&h);
  //
  // save_texture_type(font_texture,ivec2(w,h), "test");

  FreeTypeTest freetype_test;
  freetype_test.FontsFlags = ImGuiFreeType::MonoHinting;
  freetype_test.FontsMultiply = 1.35;
  bool show_freetype_window = false;

  ImVec4 *colors = ImGui::GetStyle().Colors;
  colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.54f, 0.54f, 0.54f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.84f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.00f, 0.16f, 0.26f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(1.00f, 0.25f, 0.00f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(1.00f, 0.31f, 0.00f, 0.92f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(1.00f, 0.28f, 0.00f, 1.00f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.08f, 1.00f, 0.00f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.77f, 0.08f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.87f, 1.00f, 0.00f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(1.00f, 0.38f, 0.00f, 0.59f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 0.25f, 0.00f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 0.31f, 0.00f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.94f, 0.36f, 0.00f, 0.31f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.98f, 0.40f, 0.00f, 0.80f);
  colors[ImGuiCol_HeaderActive] = ImVec4(1.00f, 0.53f, 0.00f, 1.00f);
  colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 0.31f, 0.00f, 0.72f);
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 0.35f, 0.00f, 1.00f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 0.25f, 0.00f, 1.00f);
  colors[ImGuiCol_Tab] = ImVec4(0.81f, 0.33f, 0.00f, 0.81f);
  colors[ImGuiCol_TabHovered] = ImVec4(1.00f, 0.35f, 0.00f, 0.84f);
  colors[ImGuiCol_TabActive] = ImVec4(0.99f, 0.25f, 0.00f, 1.00f);
  colors[ImGuiCol_TextSelectedBg] = ImVec4(0.36f, 0.15f, 0.00f, 0.96f);
  colors[ImGuiCol_NavHighlight] = ImVec4(0.99f, 0.53f, 0.00f, 1.00f);

  std::vector<SDL_Event> imgui_event_accumulator;

  std::vector<State *> states;
  states.emplace_back((State *)new Render_Test_State("Render Test State", window, window_size));

  states.emplace_back((State *)new Warg_State("Warg", window, window_size, (Session *)warg_session, char_name, team));

  states[0]->recieves_input = true;
  states[0]->draws_imgui = true;
  states[0]->imgui_event_accumulator = &imgui_event_accumulator;
  // states.emplace_back((State *)new Render_Test_State("Render Test State", window, window_size));
  // todo: support rendering multiple windows - should be easy, just do them one after another onto different windows
  // no problem right? just one opengl context - asset managers wont be sharing data, though, perhaps leaving it
  // global is best?

  // rendered state has control of the mouse as well
  State *rendered_state = states[0];

  bool first_update = false;
  float64 last_time = 0.0;
  float64 elapsed_time = 0.0;
  float64 current_time = 0.0f;
  bool imgui_frame_active = false;
  while (WARG_RUNNING)
  {
    const float64 time = get_real_time();
    elapsed_time = time - current_time;
    if (elapsed_time > 0.3)
      elapsed_time = 0.3;

    last_time = current_time;
    imgui_frame_active = false;
    float64 imgui_dt_accumulator = 0;
    while (current_time + dt < last_time + elapsed_time)
    {
      first_update = true;
      current_time += dt;
      imgui_dt_accumulator += dt;

      SDL_PumpEvents();

      // get keyboard state
      std::vector<uint8> key_state;
      int32 numkeys = 0;
      const Uint8 *keys = SDL_GetKeyboardState(&numkeys);
      key_state.resize(numkeys);
      for (uint32 i = 0; i < numkeys; ++i)
      {
        key_state[i] = keys[i];
      }

      // get mouse state
      ivec2 cursor_position;
      ivec2 mouse_delta;
      uint32 mouse_state = SDL_GetMouseState(&cursor_position.x, &cursor_position.y);
      SDL_GetRelativeMouseState(&mouse_delta.x, &mouse_delta.y);

      // gather events
      std::vector<SDL_Event> state_keyboard_events;
      std::vector<SDL_Event> state_mouse_events;
      SDL_Event e;
      while (SDL_PollEvent(&e))
      {
        input_preprocess(e, &rendered_state, states, IMGUI.context->IO.WantTextInput,
            IMGUI.context->IO.WantCaptureMouse, &imgui_event_accumulator, &state_keyboard_events, &state_mouse_events);
      }
      if (!WARG_RUNNING)
        break;

      // start imgui frame if last tick before rendering
      const bool last_state_update = !(current_time + dt < last_time + elapsed_time);
      if (last_state_update)
      {
        IMGUI.cursor_position = cursor_position;
        IMGUI.mouse_state = mouse_state;
        IMGUI.handle_input(&imgui_event_accumulator);

        if (freetype_test.UpdateRebuild())
        {
          IMGUI.invalidate_device_objects();
        }
        IMGUI.new_frame(window, imgui_dt_accumulator);
        if (show_freetype_window)
          freetype_test.ShowFreetypeOptionsWindow();

        IMGUI_TEXTURE_DRAWS.clear();
        imgui_frame_active = true;
      }

      // setup this tick
      for (uint32 i = 0; i < states.size(); ++i)
      {
        const bool this_state_gets_rendered = rendered_state == states[i];
        const bool this_state_recieves_input = states[i]->recieves_input;
        const bool this_state_draws_imgui = states[i]->draws_imgui;
        states[i]->events_this_tick.clear();
        states[i]->key_state.resize(key_state.size());
        for (uint32 j = 0; j < states[i]->key_state.size(); ++j)
        {
          states[i]->key_state[j] = uint8(0);
        }

        if (this_state_recieves_input)
        {
          if ((!IMGUI.context->IO.WantTextInput) || IMGUI.ignore_all_input)
          { // append keyboard events and state if imgui doesnt want the keyboard or we are ignoring imgui's request
            states[i]->events_this_tick.insert(
                states[i]->events_this_tick.end(), state_keyboard_events.begin(), state_keyboard_events.end());
            states[i]->key_state = key_state;
          }
          if ((!IMGUI.context->IO.WantCaptureMouse) || IMGUI.ignore_all_input)
          { // append mouse events and state if imgui doesnt want the mouse or we are ignoring imgui's request
            states[i]->events_this_tick.insert(
                states[i]->events_this_tick.end(), state_mouse_events.begin(), state_mouse_events.end());
            states[i]->mouse_state = mouse_state;
            states[i]->cursor_position = cursor_position;
            states[i]->mouse_delta = mouse_delta;
          }
          if (IMGUI.context->IO.WantCaptureMouse && !(IMGUI.ignore_all_input))
          {
            states[i]->mouse_delta = ivec2(0);
          }
        }
        states[i]->imgui_this_tick = this_state_draws_imgui && last_state_update;
      }

      // allows state threads to update the state
      for (uint32 i = 0; i < states.size(); ++i)
      {
        states[i]->tick_block = false;
      }

      // spin main thread to wait on all state threads
      for (uint32 i = 0; i < states.size(); ++i)
      {
        while (states[i]->tick_block == false)
        {
          SDL_Delay(1);
          // spin and wait
        }
        const bool this_state_draws_imgui = states[i]->draws_imgui;
        if (this_state_draws_imgui)
        {
          if (imgui_frame_active)
          {
            states[i]->draw_gui();
          }
        }
      }

      uint32 count = 0;
      for (uint32 i = 0; i < states.size(); ++i)
      {
        const bool this_state_gets_rendered = rendered_state == states[i];

        if (this_state_gets_rendered)
        {
          count++;
          if (states[i]->free_cam)
          {
            // SDL_WarpMouseInWindow(nullptr, 0.5f * CONFIG.resolution.x, 0.5f * CONFIG.resolution.y);
          }

          SDL_ShowCursor(states[i]->draw_cursor);

          SDL_SetRelativeMouseMode(SDL_bool(states[i]->mouse_relative_mode));
          if (states[i]->request_cursor_warp_to != ivec2(-1))
          {
            SDL_WarpMouseInWindow(nullptr, states[i]->request_cursor_warp_to.x, states[i]->request_cursor_warp_to.y);
            states[i]->request_cursor_warp_to = ivec2(-1);
          }
        }
      }
      set_message("count: ", s(count), 1.0f);
    }

    if (!WARG_RUNNING)
      break;

    if (!first_update)
      continue;

    rendered_state->renderer.imgui_this_tick = imgui_frame_active;
    rendered_state->render(rendered_state->current_time);

    static float32 scale_factor = 1.0f;

    if (imgui_frame_active)
    {
      ImGui::SameLine();
      if (ImGui::Button("Font config"))
      {
        show_freetype_window = !show_freetype_window;
      }
      std::string messages = get_messages();
      ImGui::Text("%s", messages.c_str());

      ImGui::DragFloat("testfontsize", &scale_factor, 0.001f);

      static bool show_demo_window = true;
      if (show_demo_window)
      {
        ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
        ImGui::ShowDemoWindow(&show_demo_window);
      }

      IMGUI.build_draw_data();
      IMGUI.render();
      IMGUI.end_frame();
    }
    else
    {
      IMGUI.render();
    }

    imgui_event_accumulator.clear();
    glEnable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, IMGUI.font_texture);
    GLint w, h;

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glViewport(0, 0, window_size.x, window_size.y);
    glDisable(GL_CULL_FACE);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    vec2 texture_size_scaling = vec2(w, h) / vec2(window_size);
    mat4 scale = glm::scale(scale_factor * vec3(texture_size_scaling,1) * vec3(1, -1, 1));

    mat4 transform = fullscreen_quad(); 
    
    transform = fullscreen_quad();

    
    mat4 T = glm::translate(vec3());
    transform = fullscreen_quad();


    states[0]->renderer.passthrough.use();
    states[0]->renderer.passthrough.set_uniform("transform", transform);//(window_size));
    //states[0]->renderer.passthrough.set_uniform("transform",  ortho_projection(texture_size_scaling));
   // states[0]->renderer.quad.draw();
    glDisable(GL_BLEND);
    SDL_GL_SwapWindow(window);
    set_message("FRAME END", "");
    SWAP_TIMER.stop();
    FRAME_TIMER.start();

    rendered_state->performance_output();
    if (uint32(get_real_time()) % 2 == 0)
    {
      push_log_to_disk();
    }
  }

  for (uint32 i = 0; i < states.size(); ++i)
  {
    states[i]->running = false;

    while (states[i]->tick_block == false)
    {
      // spin and wait
    }
    if (states[i]->thread.joinable())
    {
      states[i]->thread.join();
    }
    delete states[i];
  }
  push_log_to_disk();
  states.clear();
  IMAGE_LOADER.running = false;
  // warg_session.end();
  IMGUI_TEXTURE_DRAWS.clear();
  IMGUI.destroy();
  SDL_Quit();
  CONFIG.save(config_filename);
  return 0;
}
