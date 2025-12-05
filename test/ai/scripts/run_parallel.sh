#!/bin/bash

MODEL=$1

# Path to your C++ executable
EXECUTABLE="./bin/test_ai_run_verification"

# Number of core pairs (15 pairs for 30 cores)
NUM_PAIRS=15

BASE_PORT=10000
BASE_EXAMPLE=0

# Function to run program on specific core
run_on_core() {
    local core=$1
    local party=$2
    local shared_args=$3
    
    taskset -c $core $EXECUTABLE $party $shared_args &
}

# Launch pairs of instances
for i in $(seq 0 $((NUM_PAIRS - 1))); do
    # Calculate core assignments (2 cores per pair)
    CORE1=$((i * 2))
    CORE2=$((i * 2 + 1))
    
    # Customize shared arguments for this pair if needed
    # Example: different data files per pair
    PAIR_PORT=$((BASE_PORT + i))
    PAIR_BASE_EXAMPLE=$((BASE_EXAMPLE + (i * 3)))
    PAIR_ARGS="$PAIR_PORT $MODEL $PAIR_BASE_EXAMPLE 3"
    
    echo "Starting pair $i:"
    echo "  Party 1 on core $CORE1"
    echo "  Party 2 on core $CORE2"
    
    # Launch party 1 and party 2 on adjacent cores
    run_on_core $CORE1 1 "$PAIR_ARGS"
    run_on_core $CORE2 2 "$PAIR_ARGS"
done

# Wait for all background processes to complete
wait

echo "All instances completed"