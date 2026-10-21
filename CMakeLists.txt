cmake_minimum_required(VERSION 3.21)

project(QingYing VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8) # 第三方源码含 UTF-8 字符，中文代码页下必须指定
endif()
include(FetchContent)
# ZXing-cpp（Apache-2.0）：二维码 / 条码识别，仅编译读取部分
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_READERS ON CACHE BOOL "" FORCE)
set(BUILD_WRITERS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_BLACKBOX_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PYTHON_MODULE OFF CACHE BOOL "" FORCE)
set(BUILD_C_API OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zxing
    GIT_REPOSITORY https://github.com/zxing-cpp/zxing-cpp.git
    GIT_TAG v2.2.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(zxing)

find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(QingYing WIN32
    src/main.cpp
    src/App.cpp src/App.h
    src/Style.cpp src/Style.h
    src/Icons.cpp src/Icons.h
    src/Settings.cpp src/Settings.h
    src/HotkeyManager.cpp src/HotkeyManager.h
    src/ScreenGrabber.cpp src/ScreenGrabber.h
    src/WindowDetector.cpp src/WindowDetector.h
    src/Annotation.cpp src/Annotation.h
    src/ToolBars.cpp src/ToolBars.h
    src/CaptureOverlay.cpp src/CaptureOverlay.h
    src/PinWindow.cpp src/PinWindow.h
    src/SettingsDialog.cpp src/SettingsDialog.h
    src/History.cpp src/History.h
    src/HistoryWindow.cpp src/HistoryWindow.h
    src/ScrollCapture.cpp src/ScrollCapture.h
    src/ResultDialog.cpp src/ResultDialog.h
    src/Ocr.cpp src/Ocr.h
    src/Recognize.cpp src/Recognize.h
    src/QrDecoder.cpp src/QrDecoder.h
    src/Recorder.cpp src/Recorder.h
    src/RecordSession.cpp src/RecordSession.h
    third_party/gif/gif.h
    resources/QingYing.rc
)
qt_add_resources(QingYing "app_icons" PREFIX "/" FILES resources/QingYing.ico resources/QingYing.png)

target_compile_definitions(QingYing PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    QINGYING_VERSION="${PROJECT_VERSION}")
target_link_libraries(QingYing PRIVATE Qt6::Widgets ZXing::ZXing user32 gdi32 dwmapi windowsapp mfplat mfreadwrite mfuuid ole32)
if(MSVC)
    target_compile_options(QingYing PRIVATE /utf-8 /W3 /permissive- /bigobj)
endif()

# 构建后自动部署 Qt 运行库，使 exe 可直接运行
get_target_property(_qmake Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qtbin "${_qmake}" DIRECTORY)
find_program(WINDEPLOYQT windeployqt HINTS "${_qtbin}")
if(WINDEPLOYQT)
    add_custom_command(TARGET QingYing POST_BUILD
        COMMAND "${WINDEPLOYQT}" --no-translations --no-compiler-runtime --no-system-d3d-compiler
                --no-opengl-sw "$<TARGET_FILE:QingYing>"
        VERBATIM)
endif()
