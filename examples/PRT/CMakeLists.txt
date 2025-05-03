add_subdirectory(precompute_transfer)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
file(GLOB SOURCES ${SOURCE_DIR}/*.cpp ${SOURCE_DIR}/*.h)

add_executable (PRT ${SOURCES})
target_link_libraries(PRT base ${Vulkan_LIBRARY} ${WINLIBS})