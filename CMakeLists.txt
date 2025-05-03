# CMakeList.txt: 顶层 CMake 项目文件，在此处执行全局配置
# 并包含子项目。
#
cmake_minimum_required (VERSION 3.10)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("GAMES202")

if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_WIN32_KHR")
else()
    message(FATAL_ERROR "Only support Windows now!")
endif()

include_directories(external)
include_directories(external/glm)
#include_directories(external/vulkan)
include_directories(external/imgui)
include_directories(external/tinygltf)
include_directories(external/ktx/include)
include_directories(external/ktx/other_include)
include_directories(base)

# vulkan
if (NOT CMAKE_VERSION VERSION_LESS 3.7.0)
    find_package(Vulkan)    # using module to find in system path
    if (Vulkan_LIBRARY)
        message(STATUS "Using Vulkan already in system path:")
        message(STATUS "Vulkan found: Version ${Vulkan_VERSION}")
    endif()
endif()

if (NOT Vulkan_FOUND)
    find_library(Vulkan_LIBRARY NAMES vulkan-1 PATHS ${CMAKE_SOURCE_DIR}/libs/vulkan)
    if (Vulkan_LIBRARY)
        message(STATUS "Using Vulkan bundled in project path:")
        set(Vulkan_FOUND ON)
    endif()
endif()

if (NOT Vulkan_FOUND)
    message(FATAL_ERROR "Fail to find Vulkan!")
else ()
    message(STATUS "    " ${Vulkan_LIBRARY})
endif()

# Set preprocessor defines
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNOMINMAX -D_USE_MATH_DEFINES")

# Clang specific stuff
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-switch-enum")
endif()


add_definitions(-D_CRT_SECURE_NO_WARNINGS)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

file(GLOB SOURCE *.cpp )

# 包含子项目。
add_subdirectory(base)
add_subdirectory(examples)
