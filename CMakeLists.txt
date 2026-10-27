cmake_minimum_required(VERSION 3.21)

project(QingYing VERSION 0.6.3 LANGUAGES CXX)

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

find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets Network LinguistTools)
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
    src/SettingsWidgets.cpp src/SettingsWidgets.h
    src/History.cpp src/History.h
    src/HistoryWindow.cpp src/HistoryWindow.h
    src/ScrollCapture.cpp src/ScrollCapture.h
    src/ScrollStitcher.cpp src/ScrollStitcher.h
    src/ResultDialog.cpp src/ResultDialog.h
    src/Translator.cpp src/Translator.h
    src/Ocr.cpp src/Ocr.h
    src/Recognize.cpp src/Recognize.h
    src/QrDecoder.cpp src/QrDecoder.h
    src/Recorder.cpp src/Recorder.h
    src/MicCapture.cpp src/MicCapture.h
    src/RecordSession.cpp src/RecordSession.h
    third_party/gif/gif.h
    src/Theme.cpp src/Theme.h
    src/Updater.cpp src/Updater.h src/Notify.h
    src/Secret.cpp src/Secret.h
    src/Uploader.cpp src/Uploader.h
    src/Logic.cpp src/Logic.h
    src/Language.cpp src/Language.h
    src/GifPalette.cpp src/GifPalette.h
    src/CursorBlend.cpp src/CursorBlend.h
    src/Diagnostics.cpp src/Diagnostics.h
    src/EditRequest.cpp src/EditRequest.h
    resources/QingYing.rc
)
# 界面翻译：源码为简体中文，en_US 编译为 .qm 嵌入资源 :/i18n
qt_add_translations(QingYing
    TS_FILES i18n/qingying_zh_CN.ts i18n/qingying_en_US.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -no-obsolete)
# Qt 自带对话框的中文翻译（来自 Qt 安装目录的 qttranslations，缺失时跳过）
set(_qtbase_qm "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}/qtbase_zh_CN.qm")
if(EXISTS "${_qtbase_qm}")
    set_source_files_properties("${_qtbase_qm}" PROPERTIES QT_RESOURCE_ALIAS qtbase_zh_CN.qm)
    qt_add_resources(QingYing "qt_i18n" PREFIX "/i18n" FILES "${_qtbase_qm}")
else()
    message(WARNING "qtbase_zh_CN.qm not found; Qt dialogs will not be translated")
endif()
qt_add_resources(QingYing "app_icons" PREFIX "/" FILES resources/QingYing.ico resources/QingYing.png)
qt_add_resources(QingYing "ui_images" PREFIX "/ui" BASE resources/ui FILES
    resources/ui/chevron-up.png resources/ui/chevron-up@2x.png resources/ui/chevron-down.png resources/ui/chevron-down@2x.png)

target_compile_definitions(QingYing PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    QINGYING_VERSION="${PROJECT_VERSION}")
target_link_libraries(QingYing PRIVATE Qt6::Widgets Qt6::Network ZXing::ZXing user32 gdi32 dwmapi windowsapp mfplat mfreadwrite mfuuid ole32 crypt32 d3d11 dxgi dbghelp mmdevapi)
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

# 单元测试（QtTest）：纯逻辑部分，ctest 运行
option(QINGYING_BUILD_TESTS "Build unit tests" ON)
if(QINGYING_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()
    qt_add_executable(qingying_tests tests/tst_logic.cpp src/Logic.cpp src/Logic.h src/ScrollStitcher.cpp src/ScrollStitcher.h src/CursorBlend.cpp src/CursorBlend.h src/GifPalette.cpp src/GifPalette.h)
    set_target_properties(qingying_tests PROPERTIES WIN32_EXECUTABLE OFF)
    target_link_libraries(qingying_tests PRIVATE Qt6::Gui Qt6::Test)
    if(MSVC)
        target_compile_options(qingying_tests PRIVATE /utf-8 /W3 /permissive-)
    endif()
    add_test(NAME logic COMMAND qingying_tests)
    set_tests_properties(logic PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qtbin}")
    qt_add_executable(qingying_scroll_bench tests/bench_scroll.cpp src/ScrollStitcher.cpp src/ScrollStitcher.h)
    set_target_properties(qingying_scroll_bench PROPERTIES WIN32_EXECUTABLE OFF)
    target_link_libraries(qingying_scroll_bench PRIVATE Qt6::Gui)
endif()
