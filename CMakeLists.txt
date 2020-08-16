cmake_minimum_required (VERSION 3.4 FATAL_ERROR)
include (FindPkgConfig)
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

project (warg)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#glbinding spams warning 4251

set (CMAKE_BUILD_TYPE Debug)
file (GLOB CPP_FILES "*.cpp")

file (GLOB HEADER_FILES "*.h" ".hpp")

if(CMAKE_BUILD_TYPE MATCHES Debug)
set(CPP_FILES ${CPP_FILES} "${CMAKE_SOURCE_DIR}/Third_party/glad/glad_debug/src/glad.cpp")
else()
set(CPP_FILES ${CPP_FILES} "${CMAKE_SOURCE_DIR}/Third_party/glad/glad/src/glad.cpp") 
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
set(HEADER_FILES ${HEADER_FILES} "${CMAKE_SOURCE_DIR}/Third_party/glad/glad_debug/include/glad/glad.h")
else()
set(HEADER_FILES ${HEADER_FILES} "${CMAKE_SOURCE_DIR}/Third_party/glad/glad/include/glad/glad.h") 
endif()




file (GLOB SHADER_FILES "Assets/Shaders/*.vert" "Assets/Shaders/*.frag")
file (GLOB THIRD_PARTY "Third_party/imgui/*.h" "Third_party/imgui/*.cpp" "Third_party/utf8/*.h" "Third_party/stb/*.h")
add_executable (warg ${CPP_FILES} ${HEADER_FILES} ${SHADER_FILES} ${THIRD_PARTY})

set_target_properties(warg PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/")

if (MSVC) 
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4251")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4305")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Gm-")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
  target_link_options(warg PUBLIC /SUBSYSTEM:WINDOWS)
endif (MSVC)



find_package (OpenGL REQUIRED)
include_directories (${OpenGL_INCLUDE_DIRS})
target_link_libraries (warg ${OPENGL_LIBRARIES})

#assimp
set(ASSIMP_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/Third_party/assimp/include")
include_directories (${ASSIMP_INCLUDE_DIRS})
set(ASSIMP_LIB_PATH "${CMAKE_SOURCE_DIR}/Third_party/assimp/x64/Release/assimp-vc140-mt.lib")
target_link_libraries (warg ${ASSIMP_LIB_PATH})

#glm
set(GLM_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/Third_party/glm")
include_directories (${GLM_INCLUDE_DIRS})

#SDL2
set(SDL2_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/Third_party/SDL2-2.0.12" "${CMAKE_SOURCE_DIR}/Third_party/SDL2-2.0.12/SDL2")
include_directories (${SDL2_INCLUDE_DIRS})
set(SDL2_LIBRARIES "${CMAKE_SOURCE_DIR}/Third_party/SDL2-2.0.12/lib/x64/SDL2.lib" "${CMAKE_SOURCE_DIR}/Third_party/SDL2-2.0.12/lib/x64/SDL2main.lib")
target_link_libraries (warg ${SDL2_LIBRARIES})

#SDL2_Image
set(SDL2_IMAGE_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/Third_party/SDL2_image-2.0.3/include")
include_directories (${SDL2_IMAGE_INCLUDE_DIRS})
set(SDL2_IMAGE_LIBRARIES "${CMAKE_SOURCE_DIR}/Third_party/SDL2_image-2.0.3/lib/x64/SDL2_image.lib")
target_link_libraries (warg ${SDL2_IMAGE_LIBRARIES})

#enet
set(ENet_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/Third_party/enet/include")
include_directories (${ENet_INCLUDE_DIRS})
set(ENet_LIBRARIES "${CMAKE_SOURCE_DIR}/Third_party/enet/build/Release/enet.lib")
target_link_libraries (warg ${ENet_LIBRARIES})

#glad
set(GLAD_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/Third_party/glad/glad/include")
if(CMAKE_BUILD_TYPE MATCHES Debug)
set(GLAD_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/Third_party/glad/glad_debug/include")
endif(CMAKE_BUILD_TYPE MATCHES Debug)
include_directories (${GLAD_INCLUDE_DIRS})

target_include_directories (warg PRIVATE "Third_Party")
target_include_directories (warg PRIVATE "")


target_link_libraries (warg ${CMAKE_THREAD_LIBS_INIT})
#target_precompile_headers(warg PRIVATE stdafx.h)




# Enet windows dependencies
if(WIN32)
  target_link_libraries (warg wsock32 ws2_32 winmm)
endif()


set(DLL_COPIES ${CMAKE_CURRENT_SOURCE_DIR}/Third_party/assimp/x64/Release/assimp-vc140-mt.dll 
               ${CMAKE_CURRENT_SOURCE_DIR}/Third_party/SDL2-2.0.12/lib/x64/SDL2.dll
               ${CMAKE_CURRENT_SOURCE_DIR}/Third_party/SDL2-2.0.12/lib/x64/SDL2.dll
               ${CMAKE_CURRENT_SOURCE_DIR}/Third_party/SDL2_image-2.0.3/lib/x64/libjpeg-9.dll
               ${CMAKE_CURRENT_SOURCE_DIR}/Third_party/SDL2_image-2.0.3/lib/x64/libpng16-16.dll
               ${CMAKE_CURRENT_SOURCE_DIR}/Third_party/SDL2_image-2.0.3/lib/x64/libtiff-5.dll
               ${CMAKE_CURRENT_SOURCE_DIR}/Third_party/SDL2_image-2.0.3/lib/x64/libwebp-7.dll
               ${CMAKE_CURRENT_SOURCE_DIR}/Third_party/SDL2_image-2.0.3/lib/x64/SDL2_image.dll
               ${CMAKE_CURRENT_SOURCE_DIR}/Third_party/SDL2_image-2.0.3/lib/x64/zlib1.dll
               )

                
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${DLL_COPIES} $<TARGET_FILE_DIR:${PROJECT_NAME}>)

    
#file (GLOB SHADER_COPIES "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Shaders/*")

#add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${SHADER_COPIES} $<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets/Shaders/)


