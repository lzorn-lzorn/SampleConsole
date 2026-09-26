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

# ---------- 平台相关源文件 ----------
set(APP_PLATFORM_SOURCES)
if(WIN32)
    # Windows 图标资源，其它平台无法编译
    list(APPEND APP_PLATFORM_SOURCES icon_rc.rc)
endif()

# ---------- GLFW 库 ----------
add_subdirectory(glfw3)

# ---------- xerces-c（静态库，跳过 samples/tests/doc 以加快构建） ----------
# 注意：xerces-c 使用标准变量 BUILD_SHARED_LIBS 控制静态/共享，而非 XERCES_BUILD_SHARED_LIBS
set(BUILD_SHARED_LIBS OFF)
# EXCLUDE_FROM_ALL：仅编译 SampleConsole 依赖到的 xerces-c 目标，
# 跳过 xerces-c 无条件 add_subdirectory 的 samples/tests/doc
add_subdirectory(xerces-c EXCLUDE_FROM_ALL)
if(MSVC)
    target_compile_options(xerces-c PRIVATE /wd4996)
    target_compile_definitions(xerces-c PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ---------- 可执行文件 ----------
# WIN32 关键字仅影响 Windows（无控制台窗口的 GUI 程序），其它平台自动忽略
add_executable(SampleConsole WIN32
        main.cpp
        ${APP_SOURCES}
        ${IMGUI_SOURCES}
        ${GLAD_SOURCES}
        ${APP_PLATFORM_SOURCES}
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
        # xerces-c 编译时需要自动生成的 Xerces_autoconf_config.hpp，
        # 它在 CMake 配置阶段生成于构建目录（如 build/xerces-c/src）
        ${CMAKE_BINARY_DIR}/xerces-c/src
)

# 链接库
target_link_libraries(SampleConsole PRIVATE
        glfw                  # GLFW 目标（由 add_subdirectory 提供）
        OpenGL::GL
        Threads::Threads
        xerces-c
)

# 说明：XERCES_STATIC_LIBRARY 在静态构建时已由 xerces-c 生成的
# Xerces_autoconf_config.hpp 自动定义，无需在此手动添加。
