cmake_minimum_required(VERSION 3.25)

project(Engine LANGUAGES CXX)

add_library(Engine STATIC)

target_sources(Engine PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libs.cpp
)

configure_file(
  ${PROJECT_SOURCE_DIR}/config.h.in
  ${PROJECT_BINARY_DIR}/config.h
)

file(GLOB_RECURSE ENGINE_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
)
target_sources(Engine PRIVATE ${ENGINE_SRC})

target_include_directories(Engine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(Engine PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)

# Vulkan SDK
find_package(Vulkan REQUIRED)

# SDL3 (from source)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL_DOCS ON CACHE BOOL "" FORCE)
set(SDL_CAMERA  OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR  OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC  OFF CACHE BOOL "" FORCE)
set(SDL_POWER   OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG  OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI   OFF CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/SDL
                 ${CMAKE_CURRENT_BINARY_DIR}/SDL
                 EXCLUDE_FROM_ALL)

# GLM
add_subdirectory(thirdparty/glm)

target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_BINARY_DIR})

# STB
include_directories(thirdparty/stb)

# JSON
include_directories(thirdparty/json)

# CJLTF
include_directories(thirdparty/cgltf)

target_link_libraries(Engine PUBLIC SDL3::SDL3 glm)

if (TARGET Vulkan::Vulkan)
  target_link_libraries(Engine PUBLIC Vulkan::Vulkan)
else()
  target_include_directories(Engine PUBLIC ${Vulkan_INCLUDE_DIRS})
  target_link_libraries(Engine PUBLIC ${Vulkan_LIBRARIES})
endif()
