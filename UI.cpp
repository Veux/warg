#include "UI.h"
#ifdef __linux__
#elif _WIN32
#include <Windows.h>
#endif
#include "SDL_Imgui_State.h"

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
        node.texture = Texture(dir + "//" + node.path);
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
}

void File_Picker::set_dir(std::string directory)
{
  dir = directory;
  dircontents = lsdir(dir.c_str());
}

bool File_Picker::run()
{
  bool clicked = false;
  bool display = true;
  ImGui::Begin("File Picker", &display, ImVec2(586, 488), 1,
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoSavedSettings);

  std::vector<const char *> dirstrings;
  for (auto &r : dircontents)
    dirstrings.push_back(r.path.c_str());

  if (ImGui::Button("Up"))
    set_dir(s(dir, "//.."));
  ImGui::SameLine();
  closed = ImGui::Button("Close");

  ImGui::PushID(0);
  auto id0 = ImGui::GetID("Thumbnails");
  ImGui::BeginChildFrame(id0, ImVec2(570, 430), 0);
  int i = 0;
  for (i = 0; i < dircontents.size(); i++)
  {
    auto &f = dircontents[i];
    if (i % 4 != 0)
      ImGui::SameLine();
    auto id1 = s("thumb", i);
    ImGui::PushID(id1.c_str());
    auto id1_ = ImGui::GetID(id1.c_str());
    ImGui::BeginChildFrame(id1_, ImVec2(130, 141),
      ImGuiWindowFlags_NoScrollWithMouse);
    if (ImGui::ImageButton((ImTextureID)f.texture.get_handle(), ImVec2(112, 112)))
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
  return closed;
}
