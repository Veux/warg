#include "stdafx.h"
#include "UI.h"
#include "SDL_Imgui_State.h"

extern std::vector<Imgui_Texture_Descriptor> IMGUI_TEXTURE_DRAWS;

ImVec2 v(vec2 v_)
{
  ImVec2 result;
  result.x = v_.x;
  result.y = v_.y;
  return result;
}

std::vector<FS_Node> lsdir(std::string dir)
{
  std::vector<FS_Node> results;
#ifdef __linux__
  ASSERT(false);
#elif _WIN32
  std::string search_path = dir + "/*";
  WIN32_FIND_DATA fd;
  HANDLE hFind = ::FindFirstFile(search_path.c_str(), &fd);
  if (hFind != INVALID_HANDLE_VALUE)
  {
    do
    {
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
  ImGui::Begin("File Picker", &display, ImVec2(586, 488), 1, ImGuiWindowFlags_NoScrollbar);

  auto winsize = ImGui::GetWindowSize();

  std::vector<const char *> dirstrings;
  for (auto &r : dircontents)
    dirstrings.push_back(r.path.c_str());

  if (ImGui::Button("Up"))
    set_dir(s(dir, "//.."));

  ImGui::PushID(0);
  auto id0 = ImGui::GetID("Thumbnails");
  ImGui::BeginChildFrame(id0, ImVec2(winsize.x - 16, winsize.y - 58), 0);
  vec2 thumbsize = vec2(128, 128);
  ImVec2 tframesize = ImVec2(thumbsize.x + 16, thumbsize.y + 29);
  int32 num_horizontal = (int32)floor((winsize.x - 32) / (tframesize.x + 8));
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
    ImGui::BeginChildFrame(id1_, tframesize, ImGuiWindowFlags_NoScrollWithMouse);

    Texture *t = f.is_dir ? &dir_icon : &f.texture;
    t->load();
    ImGui::PushID(s("thumbbutton", i).c_str());
    if (put_imgui_texture_button(t, thumbsize))
    {
      clicked = true;
      current_item = i;
    }
    ImGui::PopID();
    ImGui::Text(f.path.c_str());
    ImGui::EndChildFrame();
    ImGui::PopID();
  }
  ImGui::EndChildFrame();
  ImGui::PopID();

  ImGui::End();

  bool picked = false;
  if (clicked)
  {
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

Layout_Grid::Layout_Grid(vec2 size_, vec2 borders_, vec2 spacing_, uint32 columns_, uint32 rows_)
{
  size = size_;
  borders = borders_;
  spacing = spacing_;
  columns = columns_;
  rows = rows_;
  element_size = (size - borders * vec2(2.f) - spacing * vec2(columns - 1, rows - 1)) / vec2(columns_, rows_);
}

Layout_Grid::Layout_Grid(
    vec2 size_, vec2 borders_, vec2 spacing_, vec2 layout, vec2 ratio_identity_lhs, float32 ratio_identity_rhs)
{
  borders = borders_;
  spacing = spacing_;
  columns = (uint32)layout.x;
  rows = (uint32)layout.y;

  vec2 max_possible_element_size = (size_ - borders * vec2(2) - (layout - vec2(1)) * spacing) / layout;
  vec2 max_possible_lhs_size =
      ratio_identity_lhs * max_possible_element_size + (ratio_identity_lhs - vec2(1)) * spacing;
  float32 ratio_of_max_possible_lhs = max_possible_lhs_size.y / max_possible_lhs_size.x;
  vec2 final_lhs_size;
  if (ratio_of_max_possible_lhs > ratio_identity_rhs)
    final_lhs_size = vec2(max_possible_lhs_size.x, max_possible_lhs_size.x * ratio_identity_rhs);
  else
    final_lhs_size = vec2(max_possible_lhs_size.y / ratio_identity_rhs, max_possible_lhs_size.y);
  element_size = (final_lhs_size - (ratio_identity_lhs - vec2(1)) * spacing) / ratio_identity_lhs;

  size = borders * vec2(2) + spacing * (layout - vec2(1)) + element_size * layout;
}

vec2 Layout_Grid::get_total_size()
{
  return size;
}

vec2 Layout_Grid::get_position(uint32 column, uint32 row)
{
  ASSERT(column < columns);
  ASSERT(row < rows);

  return borders + vec2(column, row) * (spacing + element_size);
}

vec2 Layout_Grid::get_section_size(uint32 number_columns, uint32 number_rows)
{
  return element_size + vec2(number_columns - 1, number_rows - 1) * (element_size + spacing);
}
