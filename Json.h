#pragma once
#include "Render.h"
#include "Scene_Graph.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#define JSON_INDENT 2

// Note: Use imgui to add things, rather than constructor code, because
// you will keep getting more duplicates every time you relaunch the game

// Will only save and restore nodes that are flagged
// Scene_Graph_Node::include_in_next_save = true;  Remember to set this flag for
// Scene_Graph::root and all nodes in the chain  The nodes to be saved must also
// be owned by their parent, use set_parent(node,parent,true);  on nodes you wish
// to be included in the json.  Note: in order to delete those nodes, you must
// explicitly remove them from the parent's owned_children vector, or call
// set_parent(node,parent,false) and drop the Node_Ptr handle;
json jsonify(Scene_Graph &scene);

// does the magical dejsonificationing, will dump the json to warg_log if it
// fails and default-construct the scene
void dejsonificate(Scene_Graph *scene, json j);


void pretty_json(std::string& pretty_json, const std::string& json_string, std::string::size_type& pos, size_t le = 0, size_t indent = JSON_INDENT);

