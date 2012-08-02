cmake_minimum_required(VERSION 2.6)

project(WORDS_BDD)

add_definitions(-Wall -pedantic -g2 -O2 -std=c99)

link_directories("/usr/lib/cudd")

include_directories("/usr/include/cudd")

add_executable(words_bdd main.c)
target_link_libraries(words_bdd epd util mtr st cudd m)
