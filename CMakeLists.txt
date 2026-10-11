cmake_minimum_required(VERSION 3.20)
project(ember LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(EMBER_BUILD_EDITOR "Build the graphical editor (needs raylib + Dear ImGui)" ON)
set(SANITIZE "" CACHE STRING "address,undefined | thread (empty = off)")

include(FetchContent)

# ---------------------------------------------------------------------------
# Dependencies (downloaded automatically on first configure, pinned versions)
# ---------------------------------------------------------------------------
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(json)

# ---------------------------------------------------------------------------
# Engine core: pure C++, no graphics. Everything here is unit-testable.
# ---------------------------------------------------------------------------
add_library(ember_engine
  engine/src/world.cpp
  engine/src/systems.cpp
  engine/src/commands.cpp
  engine/src/level_io.cpp
  engine/src/prefabs.cpp
)
target_include_directories(ember_engine PUBLIC engine/include)
target_link_libraries(ember_engine PUBLIC nlohmann_json::nlohmann_json)
target_compile_options(ember_engine PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)

if(SANITIZE)
  target_compile_options(ember_engine PUBLIC -fsanitize=${SANITIZE} -fno-omit-frame-pointer -g)
  target_link_options(ember_engine PUBLIC -fsanitize=${SANITIZE})
endif()

add_executable(ember_tests
  tests/test_main.cpp
  tests/test_physics.cpp
  tests/test_commands.cpp
  tests/test_level_io.cpp
  tests/test_playthrough.cpp
)
target_link_libraries(ember_tests PRIVATE ember_engine)
target_compile_options(ember_tests PRIVATE -Wall -Wextra -Werror)

enable_testing()
add_test(NAME engine_tests COMMAND ember_tests)

# ---------------------------------------------------------------------------
# Editor: raylib (window, input, 2D drawing) + Dear ImGui docking branch
# (editor panels) + rlImGui (glue between the two).
# ---------------------------------------------------------------------------
if(EMBER_BUILD_EDITOR)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git GIT_TAG 5.5 GIT_SHALLOW TRUE)
  FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.92.9-docking GIT_SHALLOW TRUE)
  FetchContent_Declare(rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG 1550009359ad975927f7f0e4a3f47e3f27123ea9)
  FetchContent_MakeAvailable(raylib imgui rlimgui)

  add_library(imgui_raylib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
  )
  # SYSTEM: don't apply our strict warnings to third-party headers.
  target_include_directories(imgui_raylib SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/misc/cpp ${rlimgui_SOURCE_DIR})
  target_link_libraries(imgui_raylib PUBLIC raylib)

  add_executable(ember
    editor/src/main.cpp
    editor/src/editor_app.cpp
    editor/src/panels.cpp
    editor/src/viewport.cpp
    editor/src/renderer.cpp
  )
  target_link_libraries(ember PRIVATE ember_engine imgui_raylib)
  target_compile_options(ember PRIVATE -Wall -Wextra -Werror)
  # Use the Roboto font that ships inside the ImGui repo for a cleaner UI.
  target_compile_definitions(ember PRIVATE EMBER_FONT_PATH="${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf")
  if(APPLE)
    target_link_libraries(ember PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
  endif()
endif()
