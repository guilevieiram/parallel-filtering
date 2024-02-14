#pragma once
#include "utils.h"

void mpi_worker(int rank, void (img*));
void mpi_server(int n_workers, int n_images, img *images, int root);
