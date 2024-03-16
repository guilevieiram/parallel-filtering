#!/bin/bash

ns=(1 5 10)
cs=(1 2 4)
prods=("default" "mpi" "omp")
procs=("default" "omp" "opt")
niters=5

for i in {1..$niters}; do
for n in "${ns[@]}"; do
for c in "${cs[@]}"; do
for prod in "${prods[@]}"; do
for proc in "${procs[@]}"; do
  log="./logs/${n}_${c}_${prod}_${proc}.log"
  ./run_test.sh \
    -l $log \
    -n $n \
    -c $c \
    -p $prod \
    -j $proc
done
done
done
done
done
