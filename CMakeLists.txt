cmake_minimum_required(VERSION 3.20)

project(Game
  VERSION 1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glslang REQUIRED)

add_executable(bkthing
  src/main.cpp 
  src/Window.cpp
  src/Game.cpp
  src/Rend/Renderer.cpp
  src/Logger.cpp
  src/vma.cpp
  src/Rend/Pipeline.cpp
)

target_include_directories(bkthing PRIVATE
  vendor/include 
)

target_link_libraries(sor PRIVATE
  Vulkan::Vulkan
  glfw
  glslang::glslang
  glslang::glslang-default-resource-limits
  glslang::SPIRV
  /home/asus/vulkan/1.4.335.0/x86_64/lib/libvolk.a
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(bkthing PRIVATE DEBUG)
endif()

target_compile_definitions(bkthing PRIVATE 
  GLFW_INCLUDE_VULKAN
  VK_NO_PROTOTYPES
)


