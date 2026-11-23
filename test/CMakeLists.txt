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
    # A longer timeout than run's default, which the slowest tests exceed in
    # Debug builds on slow runners.
    set_tests_properties(${folder}_${name} PROPERTIES
                         ENVIRONMENT "EMP_PORT=${port};EMP_RUN_TIMEOUT=600")
endfunction()

# Boolean ZK
add_test_case_with_run(bool example)
# A BooleanContext circuit run in ZK (ZKBoolContext / ZKBoolSession), validated
# bit-for-bit against a ClearCtx replay of the same circuit.
add_test_case_with_run(bool test_context_zk)
add_test_case_with_run(bool bool_io)
add_test_case_with_run(bool bool_io_channel)
add_test_case_with_run(bool perm_proof)
add_test_case_with_run(bool polynomial)
add_test_case_with_run(bool inner_prdt)
# ZK SHA-256 on the IR-replay model: the sha256_256 builtin replayed over the ZK
# context and validated against a ClearCtx replay.
add_test_case_with_run(bool sha256)

# The Boolean checks against a cheating prover (bool/cheat.cpp); without
# arguments it runs the honest cases at the batch boundaries.
add_test_case_with_run(bool cheat)
# One more run of test_bool_cheat with arguments. With `regex`, the run must
# abort with that message (the verifier's, or both parties' for a wrong BoolIO
# flag): ./run then exits nonzero, so the test passes on the message and fails
# if the verifier finishes. With "" it is an ordinary run that must succeed.
function(add_bool_cheat_case name regex)
    set(test bool_cheat_${name})
    add_test(NAME ${test}
             COMMAND ${PROJECT_SOURCE_DIR}/run $<TARGET_FILE:test_bool_cheat> ${ARGN}
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    next_test_port(port)
    set_tests_properties(${test} PROPERTIES
                         ENVIRONMENT "EMP_PORT=${port};EMP_RUN_TIMEOUT=600")
    if(regex)
        set_tests_properties(${test} PROPERTIES
                             PASS_REGULAR_EXPRESSION "${regex}"
                             FAIL_REGULAR_EXPRESSION "party 2: finalize passed")
    endif()
endfunction()
# AND gates: a lie in the first of two batches (caught when the second starts),
# in a batch checked at finalize, in a full batch checked at finalize, and in
# a final batch of one gate.
add_bool_cheat_case(and_mid   "emp_zk_bool AND batch check" and 1048577 1048575)
add_bool_cheat_case(and_end   "emp_zk_bool AND batch check" and 1000 500)
add_bool_cheat_case(and_full  "emp_zk_bool AND batch check" and 1048576 1048575)
add_bool_cheat_case(and_last  "emp_zk_bool AND batch check" and 1048577 1048576)
add_bool_cheat_case(reveal    "emp-zk-bool finalize" reveal 1)
# f2k: a lie in the first chunk of the check, and in the last of three.
add_bool_cheat_case(f2k       "emp-zk-bool f2k mult batch check" f2k 10 3)
add_bool_cheat_case(f2k_late  "emp-zk-bool f2k mult batch check" f2k 3000 2500)
foreach(kind inner eq eq2 multi)
    add_bool_cheat_case(poly_${kind} "boolean polynomial zkp fails" poly ${kind} 0)
endforeach()
add_bool_cheat_case(padding "padding of a closed chunk must be all true" padding)
add_bool_cheat_case(role    "sender flag must be true on the prover" role)
# Honest runs that cross a SilentFerret round: streaming, where the next round
# is prepaid as it starts, and a prepay of several rounds, which rolls into the
# next one without wire traffic.
add_bool_cheat_case(rollover        "" rollover 0)
add_bool_cheat_case(rollover_prepay "" rollover 40000000)
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
