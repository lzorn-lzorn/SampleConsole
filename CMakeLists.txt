cmake_minimum_required(VERSION 3.21)
project(SampleConsole)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# ---- 静态链接 MSVC 运行时库（避免依赖 MSVCP140.dll 等） ----
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()


# ---------- 查找依赖 ----------
find_package(OpenGL REQUIRED)          # 提供 OpenGL::GL
find_package(Threads REQUIRED)         # GLFW 内部需要线程

# ---- GLFW 强制静态库 ----
set(GLFW_BUILD_STATIC ON CACHE BOOL "Build GLFW as a static library" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Don't build GLFW examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Don't build GLFW tests" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Don't build GLFW docs" FORCE)



# ---------- ImGui 源文件 ----------
set(IMGUI_SOURCES
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_opengl3.cpp
        imgui/ImGuiFileDialog.cpp
        imgui/misc/cpp/imgui_stdlib.cpp
)

# ---------- OpenGL 加载器 (glad) ----------
set(GLAD_SOURCES
        glad/src/glad.c
)

# ---------- 应用层源文件 ----------
set(APP_SOURCES
        src/App.cpp
        src/Console.cpp
        src/MenuBar.cpp
        src/SearchPanel.cpp
        src/ShortcutManager.cpp
        src/XmlViewer.cpp
)

# ---------- GLFW 库 ----------
add_subdirectory(glfw3)

# ---------- 自动获取 xerces‑c 源码 ----------
#include(FetchContent)
#
## 可选：设置 xerces‑c 的构建选项（在下载前设定好）
set(XERCES_BUILD_SHARED_LIBS OFF CACHE BOOL "Build xerces-c as static lib" FORCE)
set(XERCES_BUILD_SAMPLES OFF CACHE BOOL "Don't build samples" FORCE)
set(XERCES_BUILD_TESTS OFF CACHE BOOL "Don't build tests" FORCE)
set(XERCES_BUILD_DOCS OFF CACHE BOOL "Don't build docs" FORCE)
#
#FetchContent_Declare(
#  xerces-c
#  GIT_REPOSITORY https://github.com/apache/xerces-c.git
#  GIT_TAG        v3.3.0
#)
#
#FetchContent_MakeAvailable(xerces-c)

add_subdirectory(xerces-c)
if (MSVC)
    target_compile_options(xerces-c PRIVATE
        /wd4996
    )
    # 如果是 CRT 弃用警告，配套加这个宏
    target_compile_definitions(xerces-c PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
endif()
# ---------- 可执行文件 ----------
add_executable(SampleConsole WIN32
        main.cpp
        ${APP_SOURCES}
        ${IMGUI_SOURCES}
        ${GLAD_SOURCES}
        icon_rc.rc
)


if(MSVC)
        set_target_properties(SampleConsole PROPERTIES
                LINK_FLAGS "/ENTRY:mainCRTStartup"
        )
        # 强制 UTF-8 编码(解决中文警告及语法错乱)
        target_compile_options(SampleConsole PRIVATE /utf-8)
endif()

# 包含目录
target_include_directories(SampleConsole PRIVATE
        imgui
        imgui/backends
        glad/include          # glad 头文件
        ${OPENGL_INCLUDE_DIR}
        xerces-c/src
        # xerces-c 在编译时需要自动生成的配置文件 Xerces_autoconf_config.hpp，这个文件是在 CMake 配置阶段生成的，并存放在构建目录(如 build/xerces-c/src)中
        ${CMAKE_BINARY_DIR}/xerces-c/src
)

# 链接库
target_link_libraries(SampleConsole PRIVATE
        glfw                  # GLFW 目标（由 add_subdirectory 提供）
        OpenGL::GL
        Threads::Threads
        xerces-c
)

# Windows 下可能需要额外链接 opengl32.lib（OpenGL::GL 已处理）
if(WIN32)
    # 某些情况下 glfw 会尝试链接 opengl32，但 OpenGL::GL 已包含
endif()

if(MSVC)
    target_compile_definitions(SampleConsole PRIVATE XERCES_STATIC_LIBRARY)
endif()