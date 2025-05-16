cmake_minimum_required(VERSION 3.10)
project(rdlc_test C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

# 添加 include 目录
include_directories(
    lib/headers
    ..
    .
)

# 设置源文件
set(SOURCES
    rdlcTest.cpp
    rdlcCriticalTest.cpp
)

# 添加rdlc.c为单独的库
add_library(rdlc STATIC ../rdlc.c)

# 设置 gtest 和 gmock 静态库路径
set(GTEST_LIB ${CMAKE_SOURCE_DIR}/lib/libgtest.a)
set(GMOCK_LIB ${CMAKE_SOURCE_DIR}/lib/libgmock.a)
set(GTEST_MAIN_LIB ${CMAKE_SOURCE_DIR}/lib/libgtest_main.a)
set(GMOCK_MAIN_LIB ${CMAKE_SOURCE_DIR}/lib/libgmock_main.a)

# 添加可执行文件
add_executable(test ${SOURCES})

# 链接静态库（用完整路径）
target_link_libraries(test
    rdlc
    ${GTEST_LIB}
    ${GMOCK_LIB}
    ${GTEST_MAIN_LIB}
    ${GMOCK_MAIN_LIB}
    pthread
)
