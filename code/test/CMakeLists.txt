
include("${CMAKE_CURRENT_LIST_DIR}/../../CMakeLists.txt")

cmake_minimum_required(VERSION 3.8.2)
set(PROJECT_NAME "goboVkTriangle_test")

project(${PROJECT_NAME} CXX)

set(CMAKE_CXX_STANDARD 14)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../cmakeSearchModule/")

set(TEST_SOURCES "main.cpp")
add_executable(${PROJECT_NAME} ${TEST_SOURCES})

find_package(google_test REQUIRED)

set(LIBS "${google_test_LIBRARIES}" "pthread")
target_link_libraries(${PROJECT_NAME} ${LIBS})
target_include_directories(${PROJECT_NAME} PRIVATE ${google_test_INCLUDE_DIRS})
