#include <mpi.h>
#include <stdlib.h>
#include "utils.h"
#include "mpi_utils.h"

void mpi_worker(int rank, void (*pipe)(img*)) {
  int size = 0; // size of the incomming message
  for (;;) {
    // receive the size of the next message
    MPI_Recv(&size, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
    if (size < 0)
      return;

    // receive the package
    img_pkg pack = malloc(sizeof(int) * size);
    MPI_Recv(pack, size, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);

    // convert it to an image
    pixel *p = malloc(sizeof(int) * sizeofp(size));
    img image = {0, 0, 0, p};
    pkg2img(pack, &image, NULL);

    // run the pipeline
    pipe(&image);

    // convert back to package
    img2pkg(image, pack, rank);

    // send it back to root
    MPI_Send(pack, size, MPI_INT, 0, 0, MPI_COMM_WORLD);
    free(pack);
  }
}

void mpi_server(int n_workers, int n_images, img *images, int root) {
  /* receive and send new images in loop */
  int max_size_image = 0;
  int s;

  for (int i = 0; i < n_images; i++)
    if (max_size_image < (s = sizeofimg(images[i])))
      max_size_image = s;
  img_pkg pack = malloc(sizeof(int) * max_size_image);

  // sending initial pack of images
  for (int i = 0; i < n_workers && i < n_images; i++) {
    s = sizeofimg(images[i]);

    // gettimeofday(&t1, NULL);

    img2pkg(images[i], pack, root);

    /* gettimeofday(&t2, NULL);
    duration = (t2.tv_sec - t1.tv_sec) + ((t2.tv_usec - t1.tv_usec) / 1e6);
    printf("Conversion to pkg%f\n", duration); */

    MPI_Send(&s, 1, MPI_INT, i + 1, 0, MPI_COMM_WORLD);
    MPI_Send(pack, s, MPI_INT, i + 1, 0, MPI_COMM_WORLD);
  }

  // recv-send loop for dynamic allocation
  for (int i = 0; i < n_images; i++) {
    s = sizeofimg(images[i]);
    int sender_rank;
    MPI_Recv(pack, s, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,
             NULL);
    pkg2img(pack, &images[i], &sender_rank);

    if (i + n_workers >= n_images)
      continue;

    s = sizeofimg(images[i + n_workers]);
    img2pkg(images[i + n_workers], pack, root);
    MPI_Send(&s, 1, MPI_INT, sender_rank, 0, MPI_COMM_WORLD);
    MPI_Send(pack, s, MPI_INT, sender_rank, 0, MPI_COMM_WORLD);
  }

  free(pack);
}
