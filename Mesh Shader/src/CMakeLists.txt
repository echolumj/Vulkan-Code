# CMakeList.txt: leanOpengl 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/src/thirdParty)
set(GLAD_INCLUDE_DIR ${THIRD_PARTY_DIR}/glad)
set(SHADER_DIR ${CMAKE_SOURCE_DIR}/src/shaders)
set(GLFW_DIR ${THIRD_PARTY_DIR}/GLFW)

include_directories(
    ${THIRD_PARTY_DIR}
)

file(GLOB SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)
file(GLOB HEADERS ${CMAKE_SOURCE_DIR}/src/*.h)
file(GLOB SHADERS ${SHADER_DIR}/*.vert ${SHADER_DIR}/*.frag ${SHADER_DIR}/*.comp ${SHADER_DIR}/*.task ${SHADER_DIR}/*.mesh)

source_group("shaders" FILES ${SHADERS})

message("CMAKE_SOURCE_DIR : ${CMAKE_SOURCE_DIR}")

# 将源代码添加到此项目的可执行文件。
add_executable(${PROJECT_NAME}
    ${HEADERS}
    ${SOURCES}
    ${SHADERS}
    ${GLAD_INCLUDE_DIR}/glad.c
)

target_link_libraries(${PROJECT_NAME}
    ${GLFW_DIR}/glfw3.lib
)

# vulkan
find_package(Vulkan REQUIRED)
if(VULKAN_FOUND)
    target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARY})
else(VULKAN_FOUND)
    message(FATAL_ERROR ”VULKAN library not found”)
endif(VULKAN_FOUND)

# glfw3
#find_package(glfw3 REQUIRED)
#if(GLFW3_FOUND)
#    target_include_directories(${PROJECT_NAME} PRIVATE ${GLFW3_INCLUDE_DIR})
#    target_link_libraries(${PROJECT_NAME} ${GLFW3_LIBRARY})
#else(GLFW3_FOUND)
#    message(FATAL_ERROR ”GLFW3 library not found”)
#endif(GLFW3_FOUND)
