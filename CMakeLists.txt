cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(fontrenderer)

add_executable(font font.cc)
set_property(TARGET font PROPERTY CXX_STANDARD 20)

