
// Implemented features:
//  [X] User texture binding. Cast 'GLuint' OpenGL texture identifier as
//  void*/ImTextureID. Read the FAQ about ImTextureID in imgui.cpp.

// You can copy and use unmodified imgui_impl_* files in your project. See
// main.cpp for an example of using this. If you use this binding you'll need to
// call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(),
// ImGui::Render() and ImGui_ImplXXXX_Shutdown().

#include "SDL_Imgui_State.h"
#include "Render.h"
#include "Third_Party/imgui/imgui.h"
#include "Third_Party/imgui/imgui_internal.h"

#include "Globals.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

// This is the main rendering function that you have to implement and provide
// to ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure) Note
// that this implementation is little overcomplicated because we are
// saving/setting up/restoring every OpenGL state explicitly, in order to be
// able to run within any OpenGL engine that doesn't do so. If text or lines
// are blurry when integrating ImGui in your engine: in your Render function,
// try translating your projection matrix by (0.5f,0.5f) or (0.375f,0.375f)
void SDL_Imgui_State::render()
{
  if (!draw_data)
    return;

  // Avoid rendering when minimized, scale coordinates for retina displays
  // (screen coordinates != framebuffer coordinates)
  ImGuiIO &io = ImGui::GetIO();
  int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
  int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
  if (fb_width == 0 || fb_height == 0)
    return;
  draw_data->ScaleClipRects(io.DisplayFramebufferScale);

  // Backup GL state
  GLenum last_active_texture;
  glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint *)&last_active_texture);
  glActiveTexture(GL_TEXTURE0);
  GLint last_program;
  glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
  GLint last_texture;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
  GLint last_sampler;
  glGetIntegerv(GL_SAMPLER_BINDING, &last_sampler);
  GLint last_array_buffer;
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
  GLint last_element_array_buffer;
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
  GLint last_vertex_array;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
  GLint last_polygon_mode[2];
  glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
  GLint last_viewport[4];
  glGetIntegerv(GL_VIEWPORT, last_viewport);
  GLint last_scissor_box[4];
  glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
  GLenum last_blend_src_rgb;
  glGetIntegerv(GL_BLEND_SRC_RGB, (GLint *)&last_blend_src_rgb);
  GLenum last_blend_dst_rgb;
  glGetIntegerv(GL_BLEND_DST_RGB, (GLint *)&last_blend_dst_rgb);
  GLenum last_blend_src_alpha;
  glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint *)&last_blend_src_alpha);
  GLenum last_blend_dst_alpha;
  glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint *)&last_blend_dst_alpha);
  GLenum last_blend_equation_rgb;
  glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint *)&last_blend_equation_rgb);
  GLenum last_blend_equation_alpha;
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint *)&last_blend_equation_alpha);
  GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
  GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
  GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
  GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

  // Setup render state: alpha-blending enabled, no face culling, no depth
  // testing, scissor enabled, polygon fill
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_SCISSOR_TEST);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  // Setup viewport, orthographic projection matrix
  glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
  const float ortho_projection[4][4] = {
      {2.0f / io.DisplaySize.x, 0.0f, 0.0f, 0.0f},
      {0.0f, 2.0f / -io.DisplaySize.y, 0.0f, 0.0f},
      {0.0f, 0.0f, -1.0f, 0.0f},
      {-1.0f, 1.0f, 0.0f, 1.0f},
  };
  glUseProgram(shader_handle);
  glUniform1i(texture_location, 0);
  glUniformMatrix4fv(projection_location, 1, GL_FALSE, &ortho_projection[0][0]);
  glBindVertexArray(vao);
  glBindSampler(0, 0); // Rely on combined texture/sampler state.

  for (int n = 0; n < draw_data->CmdListsCount; n++)
  {
    const ImDrawList *cmd_list = draw_data->CmdLists[n];
    const ImDrawIdx *idx_buffer_offset = 0;

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
        (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert),
        (const GLvoid *)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx),
        (const GLvoid *)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

    for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
    {
      const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];
      if (pcmd->UserCallback)
      {
        pcmd->UserCallback(cmd_list, pcmd);
      }
      else
      {
        const uint32 tex = (uint32)pcmd->TextureId;
        const uint32 mask = 0xffff0000;
        uint32 bits = mask & tex;
        // 0x????0000
        // any bits set in here means maybe TextureId gets high
        // and we might stomp useful bits
        bool test1 = (bits xor 0x0fff0000) == 0x0fff0000;
        bool test2 = (bits xor 0xffff0000) == 0x0fff0000;
        ASSERT((test1 || test2));

        // 0x?0000000
        bool gamma_flag_set = (tex & 0xf0000000) == 0xf0000000;

        // doesnt work on the fp frame textures, theyre linear
        // and also need to be gamma corrected to srgb
        // to display properly.
        // too dark means the texture is in linear space and needs
        // to-srgb
        // if (gamma_flag_set)
        //  glEnable(GL_FRAMEBUFFER_SRGB);

        glUniform1i(gamma_location, gamma_flag_set);

        GLuint handle = tex & 0x0000ffff;

        glBindTexture(GL_TEXTURE_2D, handle);
        glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w),
            (int)(pcmd->ClipRect.z - pcmd->ClipRect.x),
            (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
        glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount,
            sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
            idx_buffer_offset);
      }
      idx_buffer_offset += pcmd->ElemCount;
    }
  }

  // Restore modified GL state
  glUseProgram(last_program);
  glBindTexture(GL_TEXTURE_2D, last_texture);
  glBindSampler(0, last_sampler);
  glActiveTexture(last_active_texture);
  glBindVertexArray(last_vertex_array);
  glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
  glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
  glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb,
      last_blend_src_alpha, last_blend_dst_alpha);
  if (last_enable_blend)
    glEnable(GL_BLEND);
  else
    glDisable(GL_BLEND);
  if (last_enable_cull_face)
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);
  if (last_enable_depth_test)
    glEnable(GL_DEPTH_TEST);
  else
    glDisable(GL_DEPTH_TEST);
  if (last_enable_scissor_test)
    glEnable(GL_SCISSOR_TEST);
  else
    glDisable(GL_SCISSOR_TEST);
  glPolygonMode(GL_FRONT_AND_BACK, (GLenum)last_polygon_mode[0]);
  glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2],
      (GLsizei)last_viewport[3]);
  glScissor(last_scissor_box[0], last_scissor_box[1],
      (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);
}
const char *SDL_Imgui_State::get_clipboard(void *)
{
  return SDL_GetClipboardText();
}

void SDL_Imgui_State::set_clipboard(void *, const char *text)
{
  SDL_SetClipboardText(text);
}

bool SDL_Imgui_State::process_event(SDL_Event *event)
{
  ImGuiIO &io = ImGui::GetIO();
  switch (event->type)
  {
    case SDL_MOUSEWHEEL:
    {
      if (event->wheel.x > 0)
        io.MouseWheelH += 1;
      if (event->wheel.x < 0)
        io.MouseWheelH -= 1;
      if (event->wheel.y > 0)
        io.MouseWheel += 1;
      if (event->wheel.y < 0)
        io.MouseWheel -= 1;
      return true;
    }
    case SDL_MOUSEBUTTONDOWN:
    {
      if (event->button.button == SDL_BUTTON_LEFT)
        mouse_pressed[0] = true;
      if (event->button.button == SDL_BUTTON_RIGHT)
        mouse_pressed[1] = true;
      if (event->button.button == SDL_BUTTON_MIDDLE)
        mouse_pressed[2] = true;
      return true;
    }
    case SDL_TEXTINPUT:
    {
      io.AddInputCharactersUTF8(event->text.text);
      return true;
    }
    case SDL_KEYDOWN:
    case SDL_KEYUP:
    {
      int key = event->key.keysym.scancode;
      IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
      io.KeysDown[key] = (event->type == SDL_KEYDOWN);
      io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
      io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
      io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
      io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
      return true;
    }
  }
  return false;
}

void SDL_Imgui_State::create_fonts_texture()
{
  // Build texture atlas
  ImGuiIO &io = ImGui::GetIO();
  unsigned char *pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width,
      &height); // Load as RGBA 32-bits for OpenGL3 demo because it is more
                // likely to be compatible with user's existing shader.

  // Upload texture to graphics system
  GLint last_texture;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
  glGenTextures(1, &font_texture);
  glBindTexture(GL_TEXTURE_2D, font_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
      GL_UNSIGNED_BYTE, pixels);

  // Store our identifier
  io.Fonts->TexID = (void *)(intptr_t)font_texture;

  // Restore state
  glBindTexture(GL_TEXTURE_2D, last_texture);
}

bool SDL_Imgui_State::create_device_objects()
{
  // Backup GL state
  GLint last_texture, last_array_buffer, last_vertex_array;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

  const GLchar *vertex_shader =
      "#version 330\n"
      "uniform mat4 ProjMtx;\n"
      "in vec2 Position;\n"
      "in vec2 UV;\n"
      "in vec4 Color;\n"
      "out vec2 Frag_UV;\n"
      "out vec4 Frag_Color;\n"
      "void main()\n"
      "{\n"
      "	Frag_UV = UV;\n"
      "	Frag_Color = Color;\n"
      "	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
      "}\n";

  const GLchar *fragment_shader =
      "#version 330\n"
      "uniform sampler2D Texture;\n"
      "uniform bool gamma_encode;\n"
      "in vec2 Frag_UV;\n"
      "in vec4 Frag_Color;\n"
      "out vec4 Out_Color;\n"
      "void main()\n"
      "{\n"
      "vec4 result = Frag_Color * texture(Texture, Frag_UV.st);\n"
      "if (gamma_encode)\n"
      "{\n"
      "  result.rgb = pow(result.rgb, vec3(1.0f / 2.2f)); \n"
      "}\n"
      "	Out_Color = result;\n"
      "}\n";

  shader_handle = glCreateProgram();
  vert_handle = glCreateShader(GL_VERTEX_SHADER);
  frag_handle = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(vert_handle, 1, &vertex_shader, 0);
  glShaderSource(frag_handle, 1, &fragment_shader, 0);
  glCompileShader(vert_handle);
  glCompileShader(frag_handle);
  glAttachShader(shader_handle, vert_handle);
  glAttachShader(shader_handle, frag_handle);
  glLinkProgram(shader_handle);

  texture_location = glGetUniformLocation(shader_handle, "Texture");
  projection_location = glGetUniformLocation(shader_handle, "ProjMtx");
  position_location = glGetAttribLocation(shader_handle, "Position");
  uv_location = glGetAttribLocation(shader_handle, "UV");
  color_location = glGetAttribLocation(shader_handle, "Color");
  gamma_location = glGetUniformLocation(shader_handle, "gamma_encode");

  glGenBuffers(1, &vbo);
  glGenBuffers(1, &element_buffer);

  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glEnableVertexAttribArray(position_location);
  glEnableVertexAttribArray(uv_location);
  glEnableVertexAttribArray(color_location);

  glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE,
      sizeof(ImDrawVert), (GLvoid *)IM_OFFSETOF(ImDrawVert, pos));
  glVertexAttribPointer(uv_location, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert),
      (GLvoid *)IM_OFFSETOF(ImDrawVert, uv));
  glVertexAttribPointer(color_location, 4, GL_UNSIGNED_BYTE, GL_TRUE,
      sizeof(ImDrawVert), (GLvoid *)IM_OFFSETOF(ImDrawVert, col));

  create_fonts_texture();

  // Restore modified GL state
  glBindTexture(GL_TEXTURE_2D, last_texture);
  glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
  glBindVertexArray(last_vertex_array);

  return true;
}

void SDL_Imgui_State::bind() { ImGui::SetCurrentContext(context); }

void SDL_Imgui_State::build_draw_data()
{
  ImGui::Render();
  draw_data = ImGui::GetDrawData();
}

void SDL_Imgui_State::invalidate_device_objects()
{
  if (vao)
    glDeleteVertexArrays(1, &vao);
  if (vbo)
    glDeleteBuffers(1, &vbo);
  if (element_buffer)
    glDeleteBuffers(1, &element_buffer);
  vao = vbo = element_buffer = 0;

  if (shader_handle && vert_handle)
    glDetachShader(shader_handle, vert_handle);
  if (vert_handle)
    glDeleteShader(vert_handle);
  vert_handle = 0;

  if (shader_handle && frag_handle)
    glDetachShader(shader_handle, frag_handle);
  if (frag_handle)
    glDeleteShader(frag_handle);
  frag_handle = 0;

  if (shader_handle)
    glDeleteProgram(shader_handle);
  shader_handle = 0;

  if (font_texture)
  {
    glDeleteTextures(1, &font_texture);
    ImGui::GetIO().Fonts->TexID = 0;
    font_texture = 0;
  }
}

SDL_Imgui_State::SDL_Imgui_State(SDL_Window *window)
{
  context = ImGui::CreateContext();
  ImGui::SetCurrentContext(context);
  init(window);
  state_io = &ImGui::GetIO();
}

bool SDL_Imgui_State::init(SDL_Window *window)
{
  // Keyboard mapping. ImGui will use those indices to peek into the
  // io.KeyDown[] array.
  ImGuiIO &io = ImGui::GetIO();
  io.KeyMap[ImGuiKey_Tab] = SDL_SCANCODE_TAB;
  io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
  io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
  io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
  io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
  io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
  io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
  io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
  io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
  io.KeyMap[ImGuiKey_Insert] = SDL_SCANCODE_INSERT;
  io.KeyMap[ImGuiKey_Delete] = SDL_SCANCODE_DELETE;
  io.KeyMap[ImGuiKey_Backspace] = SDL_SCANCODE_BACKSPACE;
  io.KeyMap[ImGuiKey_Space] = SDL_SCANCODE_SPACE;
  io.KeyMap[ImGuiKey_Enter] = SDL_SCANCODE_RETURN;
  io.KeyMap[ImGuiKey_Escape] = SDL_SCANCODE_ESCAPE;
  io.KeyMap[ImGuiKey_A] = SDL_SCANCODE_A;
  io.KeyMap[ImGuiKey_C] = SDL_SCANCODE_C;
  io.KeyMap[ImGuiKey_V] = SDL_SCANCODE_V;
  io.KeyMap[ImGuiKey_X] = SDL_SCANCODE_X;
  io.KeyMap[ImGuiKey_Y] = SDL_SCANCODE_Y;
  io.KeyMap[ImGuiKey_Z] = SDL_SCANCODE_Z;

  io.SetClipboardTextFn = set_clipboard;
  io.GetClipboardTextFn = get_clipboard;
  io.ClipboardUserData = NULL;

  sdl_cursors[ImGuiMouseCursor_Arrow] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
  sdl_cursors[ImGuiMouseCursor_TextInput] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
  sdl_cursors[ImGuiMouseCursor_ResizeAll] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
  sdl_cursors[ImGuiMouseCursor_ResizeNS] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
  sdl_cursors[ImGuiMouseCursor_ResizeEW] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
  sdl_cursors[ImGuiMouseCursor_ResizeNESW] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
  sdl_cursors[ImGuiMouseCursor_ResizeNWSE] =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);

#ifdef _WIN32
  SDL_SysWMinfo wmInfo;
  SDL_VERSION(&wmInfo.version);
  SDL_GetWindowWMInfo(window, &wmInfo);
  io.ImeWindowHandle = wmInfo.info.win.window;
#else
  (void)window;
#endif

  return true;
}

void SDL_Imgui_State::destroy()
{
  invalidate_device_objects();

  for (ImGuiMouseCursor cursor_n = 0; cursor_n < ImGuiMouseCursor_Count_;
       cursor_n++)
    SDL_FreeCursor(sdl_cursors[cursor_n]);
}

void SDL_Imgui_State::new_frame(SDL_Window *window, float64 dt)
{

  if (!font_texture)
    create_device_objects();

  ImGuiIO &io = ImGui::GetIO();

  // Setup display size (every frame to accommodate for window resizing)
  int w, h;
  int display_w, display_h;
  SDL_GetWindowSize(window, &w, &h);
  SDL_GL_GetDrawableSize(window, &display_w, &display_h);

  io.MouseDrawCursor = false;
  //  ASSERT(framebuffer);
  // if (!framebuffer->color_attachments.size())
  //  framebuffer->color_attachments.emplace_back(Texture());
  // framebuffer->color_attachments[0].format = GL_RGBA;
  // framebuffer->color_attachments[0].size = vec2(display_w, display_h);
  // framebuffer->init();

  io.DisplaySize = ImVec2((float)w, (float)h);
  io.DisplayFramebufferScale = ImVec2(
      w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);

  io.DeltaTime = (float32)dt;

  // Setup mouse inputs (we already got mouse wheel, keyboard keys &
  // characters from our event handler)
  io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
  io.MouseDown[0] =
      mouse_pressed[0] || (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT)) !=
                              0; // If a mouse press event came, always pass it
                                 // as "mouse held this frame", so we don't
                                 // miss click-release events that are shorter
                                 // than 1 frame.
  io.MouseDown[1] =
      mouse_pressed[1] || (mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
  io.MouseDown[2] =
      mouse_pressed[2] || (mouse_state & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
  mouse_pressed[0] = mouse_pressed[1] = mouse_pressed[2] = false;

  // We need to use SDL_CaptureMouse() to easily retrieve mouse coordinates
  // outside of the client area. This is only supported from SDL 2.0.4
  // (released Jan 2016)
#if (SDL_MAJOR_VERSION >= 2) && (SDL_MINOR_VERSION >= 0) &&                    \
    (SDL_PATCHLEVEL >= 4)
  if ((SDL_GetWindowFlags(window) &
          (SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_MOUSE_CAPTURE)) != 0)
    io.MousePos = ImVec2((float)mouse_position.x, (float)mouse_position.y);
  bool any_mouse_button_down = false;
  for (int n = 0; n < IM_ARRAYSIZE(io.MouseDown); n++)
    any_mouse_button_down |= io.MouseDown[n];
  if (any_mouse_button_down &&
      (SDL_GetWindowFlags(window) & SDL_WINDOW_MOUSE_CAPTURE) == 0)
    SDL_CaptureMouse(SDL_TRUE);
  if (!any_mouse_button_down &&
      (SDL_GetWindowFlags(window) & SDL_WINDOW_MOUSE_CAPTURE) != 0)
    SDL_CaptureMouse(SDL_FALSE);
#else
  if ((SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0)
    io.MousePos = ImVec2((float)mx, (float)my);
#endif

  // Hide OS mouse cursor if ImGui is drawing it
  ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
  if (io.MouseDrawCursor || cursor == ImGuiMouseCursor_None)
  {
    SDL_ShowCursor(1);
  }
  else
  {
    SDL_SetCursor(sdl_cursors[cursor]);
    SDL_ShowCursor(1);
  }

  // Start the frame. This call will update the io.WantCaptureMouse,
  // io.WantCaptureKeyboard flag that you can use to dispatch inputs (or not)
  // to your application.
  ImGui::NewFrame();
  draw_data = nullptr;
}

void SDL_Imgui_State::end_frame()
{
  ASSERT(ImGui::GetCurrentContext() == context);
  ImGui::EndFrame();
}

void SDL_Imgui_State::handle_input(std::vector<SDL_Event> *input)
{
  ASSERT(input);
  mouse_state = SDL_GetMouseState(&mouse_position.x, &mouse_position.y);

  if (ignore_all_input)
  {
    mouse_position = ivec2(0);
    mouse_state = 0;
    return;
  }

  for (auto &e : *input)
  {
    process_event(&e);
  }
}
