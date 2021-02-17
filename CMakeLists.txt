cmake_minimum_required (VERSION 2.8.11)
project (emp-zk)
set(NAME "emp-zk")

find_path(CMAKE_FOLDER NAMES cmake/emp-tool-config.cmake)
include(${CMAKE_FOLDER}/cmake/common.cmake)
include(${CMAKE_FOLDER}/cmake/enable_rdseed.cmake)
include(${CMAKE_FOLDER}/cmake/enable_float.cmake)

find_package(emp-ot REQUIRED)
include_directories(${EMP-OT_INCLUDE_DIRS})
set(sources
  emp-zk/emp-zk-bool/emp-zk-bool.cpp
)

add_library(${NAME} SHARED ${sources})

install(DIRECTORY emp-zk DESTINATION include/)
install(DIRECTORY cmake/ DESTINATION cmake/)
install(TARGETS ${NAME} DESTINATION lib)

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
   add_executable("${bin}_bool" "test_bool/${bin}.cpp")
	target_link_libraries("${bin}_bool" ${NAME} ${EMP-OT_LIBRARIES})
endforeach()

set(binaries2  
input_scalability 
circuit_scalability 
inner_prdt
mix
polynomial
zk_proof
vole_triple
ostriple
cope
base_svole)

foreach(bin ${binaries2})
   add_executable("${bin}_arith" "test_arith/${bin}.cpp")
	target_link_libraries("${bin}_arith" ${NAME} ${EMP-OT_LIBRARIES})
endforeach()
