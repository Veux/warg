#pragma once

#include "Globals.h"
#include "Render.h"
#include "SDL_Imgui_State.h"

struct FS_Node
{
  std::string path;
  bool is_dir;
  Texture texture;
};

class File_Picker
{
public:
  File_Picker(const char *directory);
  bool run();
  std::string get_result();
  bool get_closed();
private:
  void set_dir(std::string directory);

  std::string dir;
  std::vector<FS_Node> dircontents;
  size_t ndirs = 0;
  int current_item = 0;
  std::string result;
  bool display = true;
  Texture dir_icon;
};

class Layout_Grid
{
public:
  Layout_Grid(vec2 size_, vec2 borders_, vec2 spacing_, uint32 columns_, uint32 rows_);
  Layout_Grid(vec2 size_, vec2 borders_, vec2 spacing_, vec2 layout, vec2 ratio_identity_lhs, float32 ratio_identity_rhs);
  vec2 get_position(uint32 column, uint32 row);
  vec2 get_section_size(uint32 number_columns, uint32 number_rows);
  vec2 get_total_size();

private:
  vec2 size = vec2(0, 0);
  vec2 borders = vec2(0, 0);
  vec2 spacing = vec2(0, 0);
  uint32 columns = 0;
  uint32 rows = 0;
  vec2 element_size = vec2(0, 0);
};

ImVec2 v(vec2 v_);
