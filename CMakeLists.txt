# Basic Vulkan "Hello Triangle" example.
# Copyright 2017-2018  Szilard Orban, <devszilardo@gmail.com>
# All rights reserved.

cmake_minimum_required(VERSION 3.10)
project("vkTriangle" VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)

add_definitions("-DDEVELOPMENT_BUILD")

set(VK_TRIANGLE_PUBLIC_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/code/public/include/goboVkTriangle/goboVkTriangle.h")
set(VK_TRIANGLE_SRC "${CMAKE_CURRENT_LIST_DIR}/code/src/goboVkTriangle.cpp")

add_executable(${PROJECT_NAME}  ${VK_TRIANGLE_SRC} ${VK_TRIANGLE_PUBLIC_HEADERS} ${VK_TRIANGLE_PRIVATE_HEADERS})

find_package(Vulkan REQUIRED)
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif()
target_include_directories(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)

set(INSTALL_TARGET_TYPE "")
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER ${VK_TRIANGLE_PUBLIC_HEADERS})
target_include_directories(${PROJECT_NAME} PUBLIC
   $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/code/public/include>
   $<INSTALL_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/code/public/include>
   PRIVATE ${CMAKE_CURRENT_LIST_DIR}/code/private/include)
target_link_libraries(${PROJECT_NAME} sorban sorban_loom glm glfw Vulkan::Vulkan)
install(TARGETS ${PROJECT_NAME}  ${INSTALL_TARGET_TYPE} DESTINATION "bin"
    PUBLIC_HEADER DESTINATION "include/goboVkTriangle")

