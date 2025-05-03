# CMakeList.txt: PCSS 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
file(GLOB SOURCES ${SOURCE_DIR}/*.cpp ${SOURCE_DIR}/*.h)

add_executable (PCSS ${SOURCES})
target_link_libraries(PCSS base ${Vulkan_LIBRARY} ${WINLIBS})

# TODO: 如有需要，请添加测试并安装目标。
