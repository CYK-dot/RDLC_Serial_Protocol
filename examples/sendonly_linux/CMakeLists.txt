cmake_minimum_required(VERSION 3.10)
project(rdlc_demo C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# 头文件路径
include_directories(
	./
	../../
)

# 源文件
add_executable(rdlc_demo
    ./main.c
    ./serial_port.c
    ../../rdlc.c
)

# 如果需要链接 pthread（如用到线程）
# target_link_libraries(rdlc_demo pthread)