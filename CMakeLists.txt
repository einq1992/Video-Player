cmake_minimum_required(VERSION 3.0.0)
project(fddd VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
enable_testing()

add_executable(fddd source/main.cpp)
target_link_libraries(fddd SDL2 avutil avformat avcodec swscale swresample z m)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
