function(add_test_case folder name)
    set(target test_${folder}_${name})
    add_executable(${target} ${folder}/${name}.cpp)
    target_link_libraries(${target} PRIVATE emp-zk::emp-zk)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    add_test(NAME ${folder}_${name}
             COMMAND ${target}
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endfunction()

# Two-party tests run under ./run, which takes the port from EMP_PORT. Each gets
# its own port, so `ctest -j` never runs two of them on one port. The ports
# start at 23000, below the range the OS assigns to outgoing connections (32768
# and up on Linux, 49152 and up on macOS), so no outgoing connection can hold a
# test's port.
set_property(GLOBAL PROPERTY EMP_ZK_TEST_NEXT_PORT 23000)
function(next_test_port out)
    get_property(port GLOBAL PROPERTY EMP_ZK_TEST_NEXT_PORT)
    math(EXPR next "${port} + 1")
    set_property(GLOBAL PROPERTY EMP_ZK_TEST_NEXT_PORT ${next})
    set(${out} ${port} PARENT_SCOPE)
endfunction()

function(add_test_case_with_run folder name)
    set(target test_${folder}_${name})
    add_executable(${target} ${folder}/${name}.cpp)
    target_link_libraries(${target} PRIVATE emp-zk::emp-zk)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    add_test(NAME ${folder}_${name}
             COMMAND ${PROJECT_SOURCE_DIR}/run $<TARGET_FILE:${target}>
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    next_test_port(port)
    set_tests_properties(${folder}_${name} PROPERTIES ENVIRONMENT "EMP_PORT=${port}")
endfunction()

# Boolean ZK
add_test_case_with_run(bool example)
# A BooleanContext circuit run in ZK (ZKBoolContext / ZKBoolSession), validated
# bit-for-bit against a ClearCtx replay of the same circuit.
add_test_case_with_run(bool test_context_zk)
add_test_case_with_run(bool bool_io)
add_test_case_with_run(bool perm_proof)
add_test_case_with_run(bool polynomial)
add_test_case_with_run(bool inner_prdt)
# ZK SHA-256 on the IR-replay model: the sha256_256 builtin replayed over the ZK
# context and validated against a ClearCtx replay.
add_test_case_with_run(bool sha256)
# (input/circuit/f2k/memory _scalability are build-only benchmarks — see ../bench)

# Arithmetic ZK
add_test_case_with_run(arith abconversion)
# Edabit setup is fixed-cost (~10 min release / ~hours debug); skip in
# fast CI via `ctest -LE slow`.
set_tests_properties(arith_abconversion PROPERTIES LABELS slow)
add_test_case_with_run(arith zk_proof)
add_test_case_with_run(arith ostriple)
# (input/circuit _scalability are build-only benchmarks — see ../bench)
add_test_case_with_run(arith polynomial)
add_test_case_with_run(arith inner_prdt)
add_test_case_with_run(arith matrix_mul)
add_test_case_with_run(arith sis)

# RAM ZK
add_test_case_with_run(ram-zk ram_test)
add_test_case_with_run(ram-zk rom_test)
add_test_case_with_run(ram-zk set_test)
