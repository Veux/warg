#pragma once

#include "Globals.h"
#include "Render.h"

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
};
