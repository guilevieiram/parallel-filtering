# How to parallel with MPI

## bottlenecks
- loading images (1MB taking 1sec to load)
- redundant channels / memory layout

- each filter can be parallelized (CUDA)
- IO operations


## MPI
- workers (unite) -> distributed memory

