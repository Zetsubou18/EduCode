cmake_minimum_required(VERSION 3.21)
if(WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "F:/vcpkg/scripts/buildsystems/vcpkg.cmake")
  set(CMAKE_TOOLCHAIN_FILE "F:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
endif()
project(EduCode VERSION 0.2.0 LANGUAGES CXX)
# Older MSVC CMake detection can decode Russian UTF-8 /showIncludes as OEM text.
# Ninja must match the actual compiler prefix to track header dependencies.
if(MSVC AND CMAKE_CXX_CL_SHOWINCLUDES_PREFIX MATCHES "╨|╤")
  set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "Примечание: включение файла:")
  set(CMAKE_CL_SHOWINCLUDES_PREFIX "${CMAKE_CXX_CL_SHOWINCLUDES_PREFIX}")
endif()
enable_testing()
add_subdirectory(EduCode)
