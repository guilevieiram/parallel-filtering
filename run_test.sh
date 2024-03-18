#!/bin/bash
n=1
N=1
c=1
PROD=""
PROC=""
LOG_FILE="sobelf.log"
INPUT_DIR=images/original
OUTPUT_DIR=images/processed

# Parse arguments
while getopts ":n:N:c:p:j:l:i:o:" opt; do
  case $opt in
    n) n=$OPTARG ;;
    N) N=$OPTARG ;;
    c) c=$OPTARG ;;
    p) PROD=$OPTARG ;;
    j) PROC=$OPTARG ;;
    l) LOG_FILE=$OPTARG ;;
    i) INPUT_DIR=$OPTARG ;;
    o) OUTPUT_DIR=$OPTARG ;;
    \?) echo "Invalid option -$OPTARG" >&2
        exit 1
        ;;
    :) echo "Option -$OPTARG requires an argument." >&2
       exit 1
       ;;
  esac
done

make;

mkdir $OUTPUT_DIR 2>/dev/null
export OMP_NUM_THREADS=$c
for i in $INPUT_DIR/*gif ; do
    DEST=$OUTPUT_DIR/`basename $i .gif`-sobel.gif
    echo "Running test on $i -> $DEST"
    salloc -N $N -n $n -c $c mpirun --bind-to none ./sobelf $i $DEST $LOG_FILE $PROD $PROC
done
