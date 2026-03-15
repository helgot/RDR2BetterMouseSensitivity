cmake_minimum_required(VERSION 3.5)
project(RDR2MouseSensitivity VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE RELEASE)

find_library(D3D12_LIBRARY d3d12 HINTS "$ENV{WindowsSdkDir}/Lib/$ENV{WindowsSDKVersion}/um/x64")
find_library(DXGI_LIBRARY dxgi HINTS "$ENV{WindowsSdkDir}/Lib/$ENV{WindowsSDKVersion}/um/x64")
find_package(Vulkan REQUIRED)

find_package(Git QUIET)

if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --always
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(GIT_COMMIT "unknown")
    set(GIT_DESCRIBE "unknown")
endif()

configure_file(
    input/version.h.in
    generated/version.h
    @ONLY
)

add_compile_definitions(
    _HAS_EXCEPTIONS=0
    _CRT_SECURE_NO_WARNINGS
    _SILENCE_ALL_CXX16_DEPRECATION_WARNINGS
)

add_subdirectory(third-party/minhook)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/third-party/minhook/include)
include_directories(${PROJECT_SOURCE_DIR}/third-party/imgui)
include_directories(${PROJECT_SOURCE_DIR}/third-party/imgui/backends)
include_directories(
    ${CMAKE_BINARY_DIR}/generated
)





add_library(RDR2BetterMouseSensitivity SHARED
    src/main.cpp    
    src/mod_main.cpp
    src/helper.cpp
    src/user_interface.cpp
    src/mod_config.cpp
    src/game_hook.cpp
    src/logger.cpp
    src/renderer_hook_vulkan.cpp
    src/renderer_hook_d3d12.cpp
    third-party/imgui/imgui.cpp
    third-party/imgui/imgui_draw.cpp
    third-party/imgui/imgui_widgets.cpp
    third-party/imgui/imgui_tables.cpp
    third-party/imgui/imgui_demo.cpp
    third-party/imgui/backends/imgui_impl_vulkan.cpp
    third-party/imgui/backends/imgui_impl_dx12.cpp
    third-party/imgui/backends/imgui_impl_win32.cpp
)

target_link_libraries(RDR2BetterMouseSensitivity ${DXGI_LIBRARY} ${D3D12_LIBRARY} minhook Vulkan::Vulkan)

set_target_properties(RDR2BetterMouseSensitivity PROPERTIES
                      OUTPUT_NAME "RDR2BetterMouseSensitivity"
                      SUFFIX ".asi")