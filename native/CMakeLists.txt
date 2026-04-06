cmake_minimum_required(VERSION 3.20)
project(Clicound LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)

# 构建为 Windows GUI 应用（WIN32 = 不显示控制台窗口）
add_executable(Clicound WIN32
    src/main.cpp
    src/config.cpp
    src/audio.cpp
    src/hook.cpp
    src/tray.cpp
    src/dialog.cpp
    src/cjson/cJSON.c
    res/resource.rc
)

# 头文件搜索路径
target_include_directories(Clicound PRIVATE src src/cjson res)

# 链接 Windows 系统库（全部 SDK 自带，零外部依赖）
target_link_libraries(Clicound PRIVATE
    user32 shell32 comdlg32 comctl32 advapi32 ole32 shlwapi
)

# Unicode 支持
target_compile_definitions(Clicound PRIVATE UNICODE _UNICODE)

# MSVC 特定：禁止 C4996 警告（sprintf_s 等），启用 UTF-8 源文件
if(MSVC)
    target_compile_options(Clicound PRIVATE /utf-8 /W3)
    # 禁用链接器自动嵌入 manifest，因为我们已经在 resource.rc 中手动嵌入了 app.manifest
    # 这避免了 CVT1100 "资源重复" 错误
    set_target_properties(Clicound PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()
