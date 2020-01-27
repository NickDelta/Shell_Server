cmake_minimum_required(VERSION 3.15)
project(Server C)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

include_directories(.)

add_executable(Server
        Server.c
        Server.h
        tokenizer.c
        tokenizer.h)
