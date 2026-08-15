cmake_minimum_required(VERSION 3.15)
project(FunScreen CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT WIN32)
    message(FATAL_ERROR "This project only builds on Windows (uses DXGI Desktop Duplication + Win32).")
endif()

# -----------------------------------------------------------------------------
# GLAD (OpenGL loader)
# -----------------------------------------------------------------------------

# Generate one at https://glad.dav1d.de/ with:
#   Language: C/C++, API: gl=3.3, Profile: Core, "Generate a loader": yes
# Unzip the result into external/glad so you have:
#   external/glad/include/glad/glad.h
#   external/glad/include/KHR/khrplatform.h
#   external/glad/src/glad.c

add_library(glad STATIC external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

add_executable(FunScreen WIN32
    src/main.cpp
    src/Capture.cpp
    src/ShaderUtils.cpp
)

target_include_directories(FunScreen PRIVATE src)
target_link_libraries(FunScreen PRIVATE glad d3d11 dxgi opengl32 gdi32 user32 shcore)

# Copy the editable shader next to the exe so the file watcher finds it
add_custom_command(TARGET FunScreen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:FunScreen>/shaders
)
