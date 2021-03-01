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
ostriple_bool
bool_io 
memory_scalability
input_scalability_bool
circuit_scalability_bool
example
sha256 
polynomial_bool
inner_prdt_bool
lowmc)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/bool)
foreach(bin ${binaries})
   add_executable("${bin}" "test/test_bool/${bin}.cpp")
	target_link_libraries("${bin}" ${NAME} ${EMP-OT_LIBRARIES})
endforeach()

set(binaries2  
input_scalability_arith
circuit_scalability_arith
inner_prdt_arith
abconversion
polynomial_arith
zk_proof
ostriple_arith
matrix_mul
sis)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/arith)
foreach(bin ${binaries2})
   add_executable("${bin}" "test/test_arith/${bin}.cpp")
	target_link_libraries("${bin}" ${NAME} ${EMP-OT_LIBRARIES})
endforeach()

set(binaries3
vole_triple
cope
base_svole
lpn)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/vole)
foreach(bin ${binaries3})
   add_executable("${bin}" "test/test_vole/${bin}.cpp")
	target_link_libraries("${bin}" ${NAME} ${EMP-OT_LIBRARIES})
endforeach()
