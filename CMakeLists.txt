cmake_minimum_required(VERSION 3.25)

project(fAInEngine LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(${PROJECT_NAME}
  source/main.cpp
  source/Game.cpp
  source/TestObject.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE include)

add_subdirectory(engine)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine)

# Copy SDL3.dll next to exe (if shared)
if (WIN32 AND TARGET SDL3::SDL3)
  get_target_property(_sdl_type SDL3::SDL3 TYPE)
  if (_sdl_type STREQUAL "SHARED_LIBRARY")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
  endif()
endif()

# --- Shaders: compile with glslc if available ---
find_program(GLSLC
  NAMES glslc
  HINTS $ENV{VULKAN_SDK}/Bin $ENV{VULKAN_SDK}/bin
)

file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/assets/shaders/*.glsl)

if (GLSLC AND SHADER_SOURCES)
  set(SPIRV_OUTPUTS "")
  foreach(S ${SHADER_SOURCES})
    get_filename_component(N ${S} NAME_WE)

     string(TOLOWER ${N} NLOW)

    # Определяем стадию по имени файла
    set(STAGE "")
    if (NLOW MATCHES ".*vert.*")
      set(STAGE vertex)
    elseif (NLOW MATCHES ".*shadow.*")
      set(STAGE vertex)
    elseif (NLOW MATCHES ".*frag.*")
      set(STAGE fragment)
    elseif (NLOW MATCHES ".*comp.*")
      set(STAGE compute)
    elseif (NLOW MATCHES ".*geom.*")
      set(STAGE geometry)
    elseif (NLOW MATCHES ".*tesc.*")
      set(STAGE tesscontrol)
    elseif (NLOW MATCHES ".*tese.*")
      set(STAGE tesseval)
    elseif (NLOW MATCHES ".*rgen.*")
      set(STAGE raygen)
    elseif (NLOW MATCHES ".*rahit.*")
      set(STAGE anyhit)
    elseif (NLOW MATCHES ".*rchit.*")
      set(STAGE closesthit)
    elseif (NLOW MATCHES ".*rmiss.*")
      set(STAGE miss)
    elseif (NLOW MATCHES ".*rint.*")
      set(STAGE intersection)
    elseif (NLOW MATCHES ".*rcall.*")
      set(STAGE callable)
    elseif (NLOW MATCHES ".*mesh.*")
      set(STAGE mesh)
    elseif (NLOW MATCHES ".*task.*")
      set(STAGE task)
    endif()

    if (STAGE STREQUAL "")
      message(FATAL_ERROR "Unknown shader stage for ${S}. Rename to *.vert/*.frag or include 'vert','frag','comp' in the name.")
    endif()

    set(OUT_SPV ${CMAKE_BINARY_DIR}/assets/shaders/${N}.spv)
    add_custom_command(
      OUTPUT ${OUT_SPV}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/assets/shaders
      COMMAND ${GLSLC} -O -fshader-stage=${STAGE} ${S} -o ${OUT_SPV}
      DEPENDS ${S}
      COMMENT "Compiling shader ${N}.glsl"
      VERBATIM
    )
    list(APPEND SPIRV_OUTPUTS ${OUT_SPV})
  endforeach()
  add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_OUTPUTS})
  add_dependencies(${PROJECT_NAME} compile_shaders)

  # Copy SPIR-V to runtime dir
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_BINARY_DIR}/assets/shaders
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/shaders
  )
else()
  message(WARNING "glslc not found -> copying raw .glsl (no offline compilation).")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets/shaders
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/shaders
  )
endif()

if (MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
