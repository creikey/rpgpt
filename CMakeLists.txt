cmake_minimum_required(VERSION 3.23)
project(rpgpt C)

set(CMAKE_C_STANDARD 11)

include_directories(.)
include_directories(gen)
include_directories(thirdparty)

add_executable(rpgpt
        main.c
)
