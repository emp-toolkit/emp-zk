./run ./bin/bool/example
./run ./bin/bool/ostriple_bool
./run ./bin/bool/bool_io
./bin/bool/memory_scalability 1 12345 10 & ./bin/bool/memory_scalability 2 12345 10
./bin/bool/input_scalability_bool 1 12345 10 & ./bin/bool/input_scalability_bool 2 12345 10
./bin/bool/circuit_scalability_bool 1 12345 10 & ./bin/bool/circuit_scalability_bool 2 12345 10
./run ./bin/bool/sha256
./bin/bool/polynomial_bool 1 12345 1000 1000 & ./bin/bool/polynomial_bool 2 12345 1000 1000
./bin/bool/inner_prdt_bool 1 12345 1000 1000 & ./bin/bool/inner_prdt_bool 2 12345 1000 1000
./run ./bin/bool/lowmc
./bin/arith/input_scalability_arith 1 12345 10 & ./bin/arith/input_scalability_arith 2 12345 10
./bin/arith/circuit_scalability_arith 1 12345 10 & ./bin/arith/circuit_scalability_arith 2 12345 10
./bin/arith/inner_prdt_arith 1 12345 1000 1000 & ./bin/arith/inner_prdt_arith 2 12345 1000 1000
./bin/arith/abconversion 1 12345 10 & ./bin/arith/abconversion 2 12345 10
./bin/arith/polynomial_arith 1 12345 1000 1000 & ./bin/arith/polynomial_arith 2 12345 1000 1000
./run ./bin/arith/zk_proof
./run ./bin/arith/ostriple_arith
./bin/arith/matrix_mul 1 12345 128 & ./bin/arith/matrix_mul 2 12345 128
./run ./bin/arith/sis
./run ./bin/vole/vole_triple
./run ./bin/vole/cope
./run ./bin/vole/base_svole
./run ./bin/vole/lpn