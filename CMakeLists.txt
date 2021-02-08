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

# Test cases
macro (add_bool_test _name)
  add_executable(${_name} "test/${_name}.cpp")
	target_link_libraries(${_name} ${EMP-OT_LIBRARIES})
endmacro()

add_bool_test(ostriple)
add_bool_test(bool_io)
add_bool_test(memory_scalability)
add_bool_test(input_scalability)
add_bool_test(circuit_scalability)
add_bool_test(example)
add_bool_test(sha256)
add_bool_test(polynomial)
add_bool_test(inner_prdt)
add_bool_test(lowmc)
