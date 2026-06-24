cmake_minimum_required(VERSION 3.16)
project(hexed LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

if(NOT WIN32)
    message(FATAL_ERROR "hexed targets Windows (Win32 + DirectX 11 backend)")
endif()

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)

if(NOT EXISTS ${IMGUI_DIR}/imgui.cpp)
    message(FATAL_ERROR
        "ImGui sources not found. Run:\n"
        "  git submodule update --init --recursive")
endif()

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
set_target_properties(imgui PROPERTIES FOLDER "ThirdParty")

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/capstone/CMakeLists.txt)
    message(FATAL_ERROR
        "Capstone sources not found. Run:\n"
        "  git submodule update --init --recursive")
endif()

set(CAPSTONE_ARCHITECTURE_DEFAULT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_X86_SUPPORT ON CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAPSTONE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/capstone EXCLUDE_FROM_ALL)
set_target_properties(capstone PROPERTIES FOLDER "ThirdParty")

set(HEXED_SOURCES
    src/main.cpp
    src/app/app.cpp
    src/core/file-buffer.cpp
    src/core/settings.cpp
    src/analysis/pe-parser.cpp
    src/analysis/string-scanner.cpp
    src/analysis/disassembler.cpp
    src/analysis/control-flow-graph.cpp
    src/ui/hex-view.cpp
    src/ui/imports-panel.cpp
    src/ui/exports-panel.cpp
    src/ui/strings-panel.cpp
    src/ui/disassembly-panel.cpp
    src/ui/graph-panel.cpp
    src/ui/struct-panel.cpp
    src/ui/settings-panel.cpp
    src/ui/theme.cpp
)
set(HEXED_HEADERS
    src/app/app.hpp
    src/core/file-buffer.hpp
    src/core/settings.hpp
    src/analysis/pe-parser.hpp
    src/analysis/string-scanner.hpp
    src/analysis/disassembler.hpp
    src/analysis/control-flow-graph.hpp
    src/ui/hex-view.hpp
    src/ui/imports-panel.hpp
    src/ui/exports-panel.hpp
    src/ui/strings-panel.hpp
    src/ui/disassembly-panel.hpp
    src/ui/graph-panel.hpp
    src/ui/struct-panel.hpp
    src/ui/settings-panel.hpp
    src/ui/theme.hpp
)

add_executable(hexed WIN32 ${HEXED_SOURCES} ${HEXED_HEADERS})
target_include_directories(hexed PRIVATE
    src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/capstone/include
)
target_link_libraries(hexed PRIVATE imgui capstone d3d11 dxgi d3dcompiler comdlg32 shell32)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PREFIX "Source"
    FILES ${HEXED_SOURCES} ${HEXED_HEADERS})

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT hexed)