#!/bin/bash

# Performance benchmark for shader loading
# Usage: ./benchmark_loading.sh /path/to/sdrinfo shader_name

SDRINFO=$1
SHADER=$2

if [ -z "$SHADER" ]; then
    echo "Usage: $0 /path/to/sdrinfo shader_name"
    exit 1
fi

echo "Benchmarking 100 loads of $SHADER..."

time for i in {1..100}; do
    $SDRINFO "$SHADER" > /dev/null
done
