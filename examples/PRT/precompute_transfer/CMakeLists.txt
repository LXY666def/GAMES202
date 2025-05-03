set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
file(GLOB SOURCES ${SOURCE_DIR}/*.cpp ${SOURCE_DIR}/*.h)

add_executable (precompute_transfer ${SOURCES})
target_link_libraries(precompute_transfer base ${Vulkan_LIBRARY} ${WINLIBS})