cmake_minimum_required (VERSION 3.0)
project (emp-zk)
set(NAME "emp-zk")

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb -fopenmp -O3 -march=native -ffast-math")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall -g2 -ggdb -fopenmp -O3 -march=native -ffast-math")

find_path(CMAKE_FOLDER NAMES cmake/emp-tool-config.cmake)
include(${CMAKE_FOLDER}/cmake/emp-base.cmake)

find_package(emp-ot REQUIRED)
include_directories(${EMP-OT_INCLUDE_DIRS})
set(sources
  emp-zk/emp-zk-bool/emp-zk-bool.cpp
  emp-zk/emp-zk-arith/emp-zk-arith.cpp
  emp-zk/emp-zk-math/ZKmath-global.cpp    # our ZKmath
  emp-zk/emp-zk-math/ZKmath-functions.cpp
)
add_library(${NAME} SHARED ${sources})

install(DIRECTORY emp-zk DESTINATION include/)
install(FILES cmake/emp-zk-config.cmake DESTINATION cmake/)
install(TARGETS ${NAME} DESTINATION lib)

ENABLE_TESTING()
ADD_SUBDIRECTORY(test)
