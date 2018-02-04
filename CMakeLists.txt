cmake_minimum_required (VERSION 3.4 FATAL_ERROR)
include (FindPkgConfig)

project (warg)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
#glbinding spams warning 4251
if (MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4251")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4305")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Gm-")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif (MSVC)
set (CMAKE_BUILD_TYPE Debug)


set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

file (GLOB CPP_FILES "*.cpp")
file (GLOB HEADER_FILES "*.h" ".hpp")
file (GLOB SHADER_FILES "Assets/Shaders/*.vert" "Assets/Shaders/*.frag")

add_executable (warg ${CPP_FILES} ${HEADER_FILES} ${SHADER_FILES})

find_package (Threads REQUIRED)
find_package (SDL2 REQUIRED)
find_package (SDL2_image REQUIRED)
find_package (ASSIMP REQUIRED)
find_package (OpenGL REQUIRED)
find_package (glm REQUIRED)
find_package (GLEW REQUIRED)
find_package (glbinding REQUIRED)
find_package (enet REQUIRED)

include_directories (${OpenGL_INCLUDE_DIRS})
include_directories (${ASSIMP_INCLUDE_DIRS})
include_directories (${GLM_INCLUDE_DIRS})
include_directories (${SDL2_INCLUDE_DIRS})
#include_directories (${GLEW_INCLUDE_DIRS})
include_directories (${SDL2_IMAGE_INCLUDE_DIRS})
include_directories (${ENet_INCLUDE_DIRS})

#someone kill me
if (WIN32)
#include_directories (${GLBINDING_INCLUDE_DIRS})
#target_link_libraries (warg ${GLBINDING_LIBRARIES})
endif (WIN32)

target_link_libraries (warg ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries (warg ${OPENGL_LIBRARIES})
target_link_libraries (warg ${SDL2_LIBRARIES})
target_link_libraries (warg ${SDL2_IMAGE_LIBRARIES})
target_link_libraries (warg ${ASSIMP_LIBRARIES})
#target_link_libraries (warg ${GLEW_LIBRARIES})
target_link_libraries (warg ${ENet_LIBRARIES})

#if (UNIX)
#i have no idea what this is, but its both adding the include dir and linking, ignore the cmake warning
target_link_libraries (warg  glbinding::glbinding)
#endif (UNIX)
