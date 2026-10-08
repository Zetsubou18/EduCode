set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 WebEngine WebChannel Concurrent Test Widgets Network)
set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_executable(EduCode WIN32
  ${ROOT}/src/core/TeacherSession.cpp ${ROOT}/src/core/TeacherSession.h
  ${ROOT}/src/platform/DemoCapture.cpp ${ROOT}/src/platform/DemoCapture.h
  ${ROOT}/src/search/SearchService.cpp ${ROOT}/src/search/SearchService.h
  ${ROOT}/src/core/Configuration.cpp ${ROOT}/src/core/Configuration.h
  ${ROOT}/src/core/ControlServer.cpp ${ROOT}/src/core/ControlServer.h
  ${ROOT}/src/core/Log.cpp ${ROOT}/src/core/Log.h
  ${ROOT}/src/packages/PackageManager.cpp ${ROOT}/src/packages/PackageManager.h
  ${ROOT}/src/ai/AiAssistant.cpp ${ROOT}/src/ai/AiAssistant.h
  ${ROOT}/src/main.cpp
  ${ROOT}/src/core/AppController.cpp ${ROOT}/src/core/AppController.h
  ${ROOT}/src/projects/PythonEnvironment.cpp ${ROOT}/src/projects/PythonEnvironment.h
  ${ROOT}/src/projects/FileSystem.cpp ${ROOT}/src/projects/FileSystem.h
  ${ROOT}/src/core/Documents.cpp ${ROOT}/src/core/Documents.h
  ${ROOT}/src/process/ProcessRunner.cpp ${ROOT}/src/process/ProcessRunner.h
  ${ROOT}/src/language/LanguageServer.cpp ${ROOT}/src/language/LanguageServer.h
  ${ROOT}/src/platform/TerminalSession.cpp ${ROOT}/src/platform/TerminalSession.h
  ${ROOT}/src/platform/WindowChrome.cpp ${ROOT}/src/platform/WindowChrome.h)
target_include_directories(EduCode PRIVATE ${ROOT}/src)
target_link_libraries(EduCode PRIVATE Qt5::Core Qt5::Gui Qt5::Qml Qt5::Quick Qt5::QuickControls2 Qt5::WebEngine Qt5::WebChannel Qt5::Concurrent Qt5::Widgets Qt5::Network)
if(UNIX AND NOT APPLE)
  find_package(X11 REQUIRED)
  target_include_directories(EduCode PRIVATE ${X11_INCLUDE_DIR})
  target_link_libraries(EduCode PRIVATE ${X11_LIBRARIES})
endif()
if(WIN32)
  target_sources(EduCode PRIVATE ${ROOT}/assets/windows.rc)
  target_link_libraries(EduCode PRIVATE dwmapi)
  add_executable(SetupGui WIN32 ${ROOT}/src/installer/SetupGui.cpp)
  target_link_libraries(SetupGui PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Concurrent shell32 ole32 advapi32)
  add_executable(SetupLauncher WIN32 ${ROOT}/src/installer/Launcher.cpp)
  target_compile_features(SetupLauncher PRIVATE cxx_std_17)
  target_link_libraries(SetupLauncher PRIVATE shell32)
  if(MSVC)
    target_compile_options(SetupGui PRIVATE /utf-8)
    target_compile_options(SetupLauncher PRIVATE /utf-8)
  endif()
endif()
if(MSVC)
  target_compile_options(EduCode PRIVATE /utf-8 /W4)
endif()
find_program(NODE_EXECUTABLE node HINTS "C:/Program Files/nodejs")
if(NOT NODE_EXECUTABLE)
  message(FATAL_ERROR "Node.js is required to bundle the editor and Pyright. Run npm ci first.")
endif()
add_custom_target(RuntimeAssets ALL COMMAND ${NODE_EXECUTABLE} ${ROOT}/tools/prepare-runtime.cjs ${ROOT} $<TARGET_FILE_DIR:EduCode>)
add_dependencies(EduCode RuntimeAssets)
install(TARGETS EduCode RUNTIME DESTINATION .)
install(DIRECTORY ${ROOT}/qml ${ROOT}/web ${ROOT}/assets ${ROOT}/tools ${ROOT}/docs ${ROOT}/plugins ${ROOT}/node_modules DESTINATION .)
add_executable(CoreTests ${ROOT}/src/core/Configuration.cpp ${ROOT}/src/core/Configuration.h ${ROOT}/src/search/SearchService.cpp ${ROOT}/src/search/SearchService.h ${ROOT}/tests/CoreTests.cpp ${ROOT}/src/core/Documents.cpp ${ROOT}/src/core/Documents.h ${ROOT}/src/projects/FileSystem.cpp ${ROOT}/src/projects/FileSystem.h ${ROOT}/src/projects/PythonEnvironment.cpp ${ROOT}/src/projects/PythonEnvironment.h ${ROOT}/src/process/ProcessRunner.cpp ${ROOT}/src/process/ProcessRunner.h)
target_include_directories(CoreTests PRIVATE ${ROOT}/src)
target_link_libraries(CoreTests PRIVATE Qt5::Core Qt5::Test Qt5::Gui Qt5::Concurrent Qt5::Qml)
if(MSVC)
  target_compile_options(CoreTests PRIVATE /utf-8)
endif()
add_test(NAME CoreTests COMMAND CoreTests)
add_executable(TeacherTests ${ROOT}/tests/TeacherTests.cpp ${ROOT}/src/core/TeacherSession.cpp ${ROOT}/src/core/TeacherSession.h ${ROOT}/src/core/Log.cpp ${ROOT}/src/core/Log.h)
target_include_directories(TeacherTests PRIVATE ${ROOT}/src)
target_link_libraries(TeacherTests PRIVATE Qt5::Core Qt5::Test Qt5::Network)
if(MSVC)
  target_compile_options(TeacherTests PRIVATE /utf-8)
endif()
add_test(NAME TeacherTests COMMAND TeacherTests)
