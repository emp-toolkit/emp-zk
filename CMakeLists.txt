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
install(DIRECTORY emp-zk-arith DESTINATION include)
install(DIRECTORY emp-vole DESTINATION include)
install(DIRECTORY edabit DESTINATION include)
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
   add_executable("${bin}_bool" "test_bool/${bin}.cpp")
	target_link_libraries("${bin}_bool" ${EMP-OT_LIBRARIES})
endforeach()

set(binaries2  
input_scalability 
circuit_scalability 
inner_prdt
mix
polynomial
zk_proof)

foreach(bin ${binaries2})
   add_executable("${bin}_arith" "test_arith/${bin}.cpp")
	target_link_libraries("${bin}_arith" ${EMP-OT_LIBRARIES})
endforeach()
