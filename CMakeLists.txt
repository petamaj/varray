cmake_minimum_required(VERSION 2.8.8)

project(varray)

add_definitions(--std=c++11 -Wall -O2)
#add_definitions(--std=c++11 -Wall -g)

file(GLOB SRC "ir.h" "ir.cpp")
add_executable(${PROJECT_NAME} ${SRC})
