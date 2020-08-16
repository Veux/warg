#include "stdafx.h"
#include "UI.h"
#include "SDL_Imgui_State.h"

std::unordered_map<std::string, Texture> FILE_PICKER_TEXTURE_CACHE;

// static std::vector<Texture> FILE_PICKER_TEXTURE_CACHE;
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
      node.path = dir + "/" + fd.cFileName;
      node.is_dir = fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
      // if (!node.is_dir)
      //{
      //  std::string path = dir + "/" + node.path;
      //  if (has_img_file_extension(path))
      //  {
      //    node.texture = path;
      //  }
      //}
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
  std::stable_sort(dircontents.begin(), dircontents.end(), [](auto &left, auto &right) {
    if ((left.is_dir && right.is_dir) || (!left.is_dir && !right.is_dir))
      return lexicographical_compare(left.path.begin(), left.path.end(), right.path.begin(), right.path.end());
    return left.is_dir > right.is_dir;
  });
}

bool File_Picker::run()
{
  bool clicked = false;
  display = true;
  ImGui::Begin("File Picker", &display, ImGuiWindowFlags_NoScrollbar);

  auto winsize = ImGui::GetWindowSize();

  // std::vector<const char *> dirstrings;
  // for (FS_Node &r : dircontents)
  //  dirstrings.push_back(r.path.c_str());

  if (ImGui::Button("Up"))
  {
    size_t i = dir.find_last_of("/\\");
    if (i != -1)
      dir.erase(i, dir.size() - i);
    set_dir(dir);
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear thumbnail cache"))
  {
    FILE_PICKER_TEXTURE_CACHE.clear();
  }

  // we cache the textures to hold on to the handle globally
  // instead of holding a handle inside the file_picker object
  // because the file_picker is
  // std::vector<Texture> last_cache = FILE_PICKER_TEXTURE_CACHE;
  // FILE_PICKER_TEXTURE_CACHE.clear();

  ImGui::PushID(0);
  ImGuiID id0 = ImGui::GetID("Thumbnails");
  ImGui::BeginChildFrame(id0, ImVec2(winsize.x - 16, winsize.y - 58), 0);
  vec2 thumbsize = vec2(128, 128);
  ImVec2 tframesize = ImVec2(thumbsize.x + 16, thumbsize.y + 29);
  int32 num_horizontal = (int32)floor((winsize.x - 32) / (tframesize.x + 8));
  if (num_horizontal == 0)
    num_horizontal = 1;
  for (size_t i = 0; i < dircontents.size(); i++)
  {
    if (i % num_horizontal != 0)
      ImGui::SameLine();
    std::string id1 = s("thumb", i);
    ImGui::PushID(id1.c_str());
    ImGuiID id1_ = ImGui::GetID(id1.c_str());
    ImGui::BeginChildFrame(id1_, tframesize, ImGuiWindowFlags_NoScrollWithMouse);

    FS_Node &f = dircontents[i];
    Texture_Descriptor td = "Assets/Icons/dir.png";
    if (!f.is_dir)
    {
      td = "Assets/Icons/file.png";
      if (has_img_file_extension(f.path))
      {
        td.name = f.path;
        td.source = f.path;
        if(has_hdr_file_extension(f.path))
        {
          td.format = GL_RGBA16F;
        }
      }
    }
    if (!FILE_PICKER_TEXTURE_CACHE.contains(td.name))
    {
      FILE_PICKER_TEXTURE_CACHE[td.name] = Texture(td);
    }

    FILE_PICKER_TEXTURE_CACHE[td.name].load();

    ImGui::PushID(s("thumbbutton", i).c_str());
    if (put_imgui_texture_button(FILE_PICKER_TEXTURE_CACHE[td.name].texture, thumbsize))
    {
      clicked = true;
      last_clicked_node = i;
    }
    ImGui::PopID();
    size_t path_i = f.path.find_last_of("/\\");
    std::string filename = f.path.substr(path_i + 1);
    ImGui::Text(filename.c_str());
    ImGui::EndChildFrame();
    ImGui::PopID();
  }
  ImGui::EndChildFrame();
  ImGui::PopID();

  ImGui::End();

  bool picked = false;
  if (clicked)
  {
    if (dircontents[last_clicked_node].is_dir)
    {
      set_dir(dircontents[last_clicked_node].path);
      last_clicked_node = -1;
      picked = false;
    }
    else
    {
      result = dircontents[last_clicked_node].path;
      picked = true;
    }
  }

  return picked;
}

std::string File_Picker::get_result()
{
  return result;
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
