cmake_minimum_required (VERSION 2.8.11)
project (emp-zk-bool)
set(NAME "emp-zk-bool")

find_path(CMAKE_FOLDER NAMES cmake/emp-tool-config.cmake)
include(${CMAKE_FOLDER}/cmake/common.cmake)
include(${CMAKE_FOLDER}/cmake/enable_rdseed.cmake)
include(${CMAKE_FOLDER}/cmake/enable_float.cmake)

find_package(emp-ot REQUIRED)
include_directories(${EMP-OT_INCLUDE_DIRS})

install(DIRECTORY emp-zk-bool DESTINATION include)
install(DIRECTORY cmake/ DESTINATION cmake)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -ggdb -Wno-ignored-attributes")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -Wno-ignored-attributes")

set(binaries  
ostriple 
bool_io 
memory_scalability
input_scalability 
circuit_scalability 
example
sha256 
polynomial 
inner_prdt 
lowmc)

foreach(bin ${binaries})
   add_executable(${bin} "test/${bin}.cpp")
	target_link_libraries(${bin} ${EMP-OT_LIBRARIES})
endforeach()
