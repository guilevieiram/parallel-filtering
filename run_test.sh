#!/bin/bash
n=1
N=1
PROD="default"
PROC="default"
LOG_FILE="sobelf.log"
INPUT_DIR=images/original
OUTPUT_DIR=images/processed

# Parse arguments
while getopts ":n:N:p:j:l:i:" opt; do
  case $opt in
    n) n=$OPTARG ;;
    N) N=$OPTARG ;;
    p) PROD=$OPTARG ;;
    j) PROC=$OPTARG ;;
    l) LOG_FILE=$OPTARG ;;
    i) IMAGES_FOLDER=$OPTARG ;;
    \?) echo "Invalid option -$OPTARG" >&2
        exit 1
        ;;
    :) echo "Option -$OPTARG requires an argument." >&2
       exit 1
       ;;
  esac
done

make;

rm -f $LOG_FILE;
mkdir $OUTPUT_DIR 2>/dev/null

for i in $INPUT_DIR/*gif ; do
    DEST=$OUTPUT_DIR/`basename $i .gif`-sobel.gif
    echo "Running test on $i -> $DEST"

    salloc -N $N -n $n mpirun --bind-to none ./sobelf $i $DEST $LOG_FILE $PROD $PROC
done
