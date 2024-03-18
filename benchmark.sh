#!/bin/bash

ns=(1 5 10)
Ns=(1 2)
cs=(1 2 4)
prods=("default" "mpi" "omp")
procs=("default" "omp" "cuda")
niters=1000

get_random_element() {
    local array=("$@")
    echo ${array[$RANDOM % ${#array[@]}]}
}

for i in $(seq 1 $niters); do
    n=$(get_random_element "${ns[@]}")
    N=$(get_random_element "${Ns[@]}")
    c=$(get_random_element "${cs[@]}")
    prod=$(get_random_element "${prods[@]}")
    proc=$(get_random_element "${procs[@]}")

    echo "n${n} N${N} c${c} prod-${prod} proc-${proc}"

    # illegal combinations
    if [ "$proc" == "default"  ] && [ "$prod" == "default"  ]; then
        n=1
        N=1
        c=1
    fi
    if [ $N -gt $n ]; then
        continue
    fi
    if [ $c -eq 1 ] && [ "$proc" == "omp" ]; then
        continue
    fi
    if [ $c -eq 1 ] && [ "$prod" == "omp" ]; then
        continue
    fi
    if [ "$proc" == "omp" ] && [ "$prod" == "omp" ]; then
        continue
    fi
    if [ $n -eq 1 ] && [ "$prod" == "mpi" ]; then
        continue
    fi

    log="./logs/${n}_${N}_${c}_${prod}_${proc}.log"
    echo "n${n} N${N} c${c} prod-${prod} proc-${proc}"
    ./run_test.sh \
        -l $log \
        -n $n \
        -N $N \
        -c $c \
        -p $prod \
        -j $proc
done
