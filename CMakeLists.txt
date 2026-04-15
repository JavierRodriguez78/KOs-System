cmake_minimum_required(VERSION 3.16)

project(KOSKernel LANGUAGES C CXX ASM)

option(KOS_INPUT_DEBUG "Enable verbose keyboard/mouse/IRQ input diagnostics" OFF)

add_subdirectory(kernel/cmake)