cmake_minimum_required(VERSION 3.21)

project(QingYing VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
    resources/QingYing.rc
)
qt_add_resources(QingYing "app_icons" PREFIX "/" FILES resources/QingYing.ico resources/QingYing.png)

target_compile_definitions(QingYing PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    QINGYING_VERSION="${PROJECT_VERSION}")
target_link_libraries(QingYing PRIVATE Qt6::Widgets user32 gdi32 dwmapi windowsapp)
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
