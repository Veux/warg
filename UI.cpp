#include "UI.h"
#ifdef __linux__
#elif _WIN32
#include <Windows.h>
#endif
#include "SDL_Imgui_State.h"
extern std::vector<Imgui_Texture_Descriptor> IMGUI_TEXTURE_DRAWS;
std::vector<FS_Node> lsdir(std::string dir)
{
  std::vector<FS_Node> results;
#ifdef __linux__ 
  ASSERT(false);
#elif _WIN32
  std::string search_path = dir + "/*";
  WIN32_FIND_DATA fd;
  HANDLE hFind = ::FindFirstFile(search_path.c_str(), &fd);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (std::string(fd.cFileName) == "." || std::string(fd.cFileName) == "..")
        continue;
      FS_Node node;
      node.path = fd.cFileName;
      node.is_dir = fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
      if (!node.is_dir)
      {
        std::string path = dir + "//" + node.path;
        if (has_img_file_extension(path))
        {
          node.texture = Texture(path);
        }
      }
      results.push_back(node);
    } while (::FindNextFile(hFind, &fd));
    ::FindClose(hFind);
  }
#endif
  return results;
}

File_Picker::File_Picker(const char *directory)
{
  set_dir(directory);
  dir_icon = Texture("../Assets/Icons/dir.png");
}

void File_Picker::set_dir(std::string directory)
{
  dir = directory;
  dircontents = lsdir(dir.c_str());
}

bool File_Picker::run()
{
  bool clicked = false;
  display = true;
  ImGui::Begin("File Picker", &display, ImVec2(586, 488), 1,
    ImGuiWindowFlags_NoScrollbar);

  auto winsize = ImGui::GetWindowSize();

  std::vector<const char *> dirstrings;
  for (auto &r : dircontents)
    dirstrings.push_back(r.path.c_str());

  if (ImGui::Button("Up"))
    set_dir(s(dir, "//.."));

  ImGui::PushID(0);
  auto id0 = ImGui::GetID("Thumbnails");
  ImGui::BeginChildFrame(id0, ImVec2(winsize.x - 16, winsize.y - 58), 0);
  auto thumbsize = ImVec2(128, 128);
  auto tframesize = ImVec2(thumbsize.x + 16, thumbsize.y + 29);
  int num_horizontal = (int)floor((winsize.x - 32) / (tframesize.x + 8));
  if (num_horizontal == 0)
    num_horizontal = 1;
  for (size_t i = 0; i < dircontents.size(); i++)
  {
    auto &f = dircontents[i];
    if (i % num_horizontal != 0)
      ImGui::SameLine();
    auto id1 = s("thumb", i);
    ImGui::PushID(id1.c_str());
    auto id1_ = ImGui::GetID(id1.c_str());
    ImGui::BeginChildFrame(id1_, tframesize,
      ImGuiWindowFlags_NoScrollWithMouse);


    
    //desired method:
    Imgui_Texture_Descriptor descriptor;
    descriptor.ptr = f.is_dir ? dir_icon.texture : f.texture.texture;
    uint32 data = 0;
    if (descriptor.ptr)
    {
      descriptor.gamma_encode = is_float_format(descriptor.ptr->get_format());
      IMGUI_TEXTURE_DRAWS.push_back(descriptor);
      data = (uint32)(IMGUI_TEXTURE_DRAWS.size() - 1) | 0xf0000000;
    }
    //if (ImGui::ImageButton((ImTextureID)data, thumbsize))
    //...
    // why does this uint32 data cause the clicking to not work correctly in imgui?
    // even if you set 'handle' to '0' instead, the clicks work fine
    // i wasn't able to fix this with another PushID

    


    uint32 handle = f.is_dir ? dir_icon.get_handle() : f.texture.get_handle();
    if (ImGui::ImageButton((ImTextureID)handle, thumbsize))
    {
      clicked = true;
      current_item = i;
    }
    ImGui::Text(f.path.c_str());
    ImGui::EndChildFrame();
    ImGui::PopID();
  }
  ImGui::EndChildFrame();
  ImGui::PopID();

  ImGui::End();

  bool picked = false;
  if (clicked) {
    if (dircontents[current_item].is_dir)
    {
      set_dir(s(dir, "//", dircontents[current_item].path));
      current_item = 0;
      picked = false;
    }
    else
    {
      result = dircontents[current_item].path;
      picked = true;
    }
  }

  return picked;
}

std::string File_Picker::get_result()
{
  return s(dir + "//" + result);
}

bool File_Picker::get_closed()
{
  return !display;
}
