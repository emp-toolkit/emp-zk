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

# One more run of an existing two-party test target with arguments. With
# `regex`, the run must abort with that message (the verifier's, or both
# parties' for a check on public data): ./run then exits nonzero, so the test
# passes on the message and fails if the verifier finishes. With "" it is an
# ordinary run that must succeed.
function(add_run_case test target regex)
    add_test(NAME ${test}
             COMMAND ${PROJECT_SOURCE_DIR}/run $<TARGET_FILE:${target}> ${ARGN}
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

# Multi-wire elements folded in several compress() calls.
add_run_case(bool_perm_proof_periodic test_bool_perm_proof "" 1000 300 periodic)

# The Boolean checks against a cheating prover (bool/cheat.cpp); without
# arguments it runs the honest cases at the batch boundaries.
add_test_case_with_run(bool cheat)
function(add_bool_cheat_case name regex)
    add_run_case(bool_cheat_${name} test_bool_cheat "${regex}" ${ARGN})
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

# The RAM, ROM, set and permutation proofs against a cheating prover
# (ram-zk/cheat.cpp); without arguments it runs the honest cases and checks
# what the verifier receives from a permutation proof.
add_test_case_with_run(ram-zk cheat)
function(add_ram_cheat_case name regex)
    add_run_case(ram-zk_cheat_${name} test_ram-zk_cheat "${regex}" ${ARGN})
endfunction()
# Each gadget names itself in its mismatch message, so a case also checks
# which check caught the prover.
set(ram_mismatch "ZKRam: the reads do not match the writes")
set(set_mismatch "ZKSet: the queries do not match the set")
set(perm_mismatch "ZKPermProof: A is not a permutation of B")
# Loads that read a later write, at bounds T that are not powers of two: a time
# field of ⌈log₂(T+1)⌉ bits would let them pass (at T = 1000 once the two loads
# are 2^10 − 1000 = 24 apart). Only the freshness set catches them.
add_ram_cheat_case(ram_future_3    "${set_mismatch}" ram_future 3 3)
add_ram_cheat_case(ram_future_1000 "${set_mismatch}" ram_future 1000 25)
add_ram_cheat_case(ram_over        "more than T accesses" ram_over)
add_ram_cheat_case(ram_stale       "${ram_mismatch}" ram_stale)
add_ram_cheat_case(ram_value       "${ram_mismatch}" ram_value)
add_ram_cheat_case(ram_wide        "${ram_mismatch}" ram_wide)
add_ram_cheat_case(rom_loop        "${ram_mismatch}" rom_loop)
add_ram_cheat_case(rom_cycle       "${ram_mismatch}" rom_cycle)
add_ram_cheat_case(rom_value       "${ram_mismatch}" rom_value)
add_ram_cheat_case(rom_over        "more than T accesses" rom_over)
add_ram_cheat_case(rom_oob         "${ram_mismatch}" rom_oob)
add_ram_cheat_case(set_over        "more than max_queries" set_over)
add_ram_cheat_case(set_nonmember   "${set_mismatch}" set_nonmember 1 3)
# Four queries of a non-member. Versions sized from T = 3 would have 2 bits,
# and these queries would close a cycle of them.
add_ram_cheat_case(set_wrap        "${set_mismatch}" set_nonmember 4 4)
add_ram_cheat_case(perm            "${perm_mismatch}" perm)
add_ram_cheat_case(perm_wide       "${perm_mismatch}" perm_wide)
# A wrong product alone, and first of a full batch plus one.
add_ram_cheat_case(product         "product check fails" product 1)
add_ram_cheat_case(product_batch   "product check fails" product 65537)
add_ram_cheat_case(unchecked_finalize "never checked" unchecked finalize)
add_ram_cheat_case(unchecked_scope    "destroyed before check" unchecked scope)
add_ram_cheat_case(contract_alias     "cannot address every cell" contract alias)
add_ram_cheat_case(contract_set_width "cannot hold every element" contract set_width)
add_ram_cheat_case(contract_payload   "payload_bits must be in" contract payload)
add_ram_cheat_case(contract_width     "must be index_sz bits wide" contract width)
