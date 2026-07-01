cmake_minimum_required(VERSION 3.10)
project(SampleConsole)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


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
)

# ---------- OpenGL 加载器 (glad) ----------
set(GLAD_SOURCES
        glad/src/glad.c
)

# ---------- GLFW 库 ----------
# 如果你的 glfw3/ 目录就是 GLFW 官方源码，可以直接加入构建
add_subdirectory(glfw3)

# ---------- 可执行文件 ----------
add_executable(console WIN32
        main.cpp
        ${IMGUI_SOURCES}
        ${GLAD_SOURCES}
        icon_rc.rc
)

if(MSVC)
    set_target_properties(console PROPERTIES
            LINK_FLAGS "/ENTRY:mainCRTStartup"
    )
endif()

# 包含目录
target_include_directories(console PRIVATE
        imgui
        imgui/backends
        glad/include          # glad 头文件
        ${OPENGL_INCLUDE_DIR} # 通常不需要，但保留
)

# 链接库
target_link_libraries(console PRIVATE
        glfw                  # GLFW 目标（由 add_subdirectory 提供）
        OpenGL::GL
        Threads::Threads
)

# Windows 下可能需要额外链接 opengl32.lib（OpenGL::GL 已处理）
if(WIN32)
    # 某些情况下 glfw 会尝试链接 opengl32，但 OpenGL::GL 已包含
endif()