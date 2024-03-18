# Parallel Image Filtering

This project aims to parallelize image filtering using different distributed and parallel programing paradigms. 

This was conducted under the INF560 course at Ecole Polytechnique by Guilherme Vieira and Joao Andreotti.

## Installation

To install the program please ensure you have on your machine
- A CUDA capable system with an NVIDIA GPU.
- MPI and the mpicc compiler.
- OpenMP library installed.
- Slurm: optional but recommended to run the provided scripts.

To install just run:
```bash
make
```

## Usage
To run the application:
```bash
# to run with optimal configurations
./sobelf path/to/input.gif path/to/output.gif path/to/logs.log 

# to choose a producer from (default, mpi, omp) 
# and a processor from (default, omp, cuda)
./sobelf path/to/input.gif path/to/output.gif path/to/logs.log mpi cuda
```


To run the application over a set of images and with a specific setup, we provide the the  `run_test.sh` script. 
```bash
./run_test.sh \
    -n 5 \                  # number of processes
    -N 2 \                  # number of machines
    -c 8 \                  # number threads per process
    -p mpi \                # chosen producer
    -j cuda \               # chosen processor (job)
    -l my_log.log \         # path to log file
    -i images/original \    # folder with input images
    -o images/processed     # folder to output images
```

All of the arguments in this script have sensible default values. Under the hood it will use `slurm` and `mpirun` to allocate a set of processes for your execution.

If no producer and no processor are passed into the script, the program will decide optimally which configuration to use.



## Benchmarking
We provide in this repo the script used to benchmark the code. This script will run all different configurations used stochastically and save logs under the `./logs` folder.

```bash
./benchmark.sh
```

