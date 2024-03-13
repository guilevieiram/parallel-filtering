/*
 * INF560
 *
 * Image Filtering Project
 */
/* Set this macro to 1 to enable debugging information */
#include <mpi.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "filters.h"
#include "utils.h"

#include "mpi_utils.h"
#include "omp_utils.h"

#define SOBELF_DEBUG 0
#define ROOT 0

enum producer {
  prod_invalid,
  prod_def,
  prod_mpi,
  prod_omp,
};

enum processor { proc_invalid, proc_def, proc_opt, proc_omp };

enum producer parse_producer(char *str) {
  if (str == NULL)
    return prod_def;
  else if (!strcmp(str, "default"))
    return prod_def;
  else if (!strcmp(str, "mpi"))
    return prod_mpi;
  else if (!strcmp(str, "omp"))
    return prod_omp;
  else
    return prod_invalid;
}

enum processor parse_processor(char *str) {
  if (str == NULL)
    return proc_def;
  else if (!strcmp(str, "default"))
    return proc_def;
  else if (!strcmp(str, "opt"))
    return proc_opt;
  else if (!strcmp(str, "omp"))
    return proc_omp;
  else
    return proc_invalid;
}

void omp_pipe(img *image) {
#pragma omp parallel
  {
#if SOBELF_DEBUG
    printf("Available threads in pipe: %d \n", omp_get_max_threads());
#endif

    omp_apply_gray_filter(image);

    /* Apply blur filter with convergence value */
    apply_blur_filter_once(image, 5, 20);

    /* Apply sobel filter on pixels */
    apply_sobel_filter_once(image);
  }
}

void opt_pipe(img *image) {
  /* Convert the pixels into grayscale */
  apply_gray_filter_once(image);

  /* Apply blur filter with convergence value */
  apply_blur_filter_once_opt(image, 5, 20);

  /* Apply sobel filter on pixels */
  apply_sobel_filter_once_opt(image);
}

void default_pipe(img *image) {
  /* Convert the pixels into grayscale */
  apply_gray_filter_once(image);

  /* Apply blur filter with convergence value */
  apply_blur_filter_once(image, 5, 20);

  /* Apply sobel filter on pixels */
  apply_sobel_filter_once(image);
}

void log_pipe(img *image) {
  struct timeval t1, t2;
  double duration;

  gettimeofday(&t1, NULL);
  /* Convert the pixels into grayscale */
  apply_gray_filter_once(image);
  gettimeofday(&t2, NULL);
  duration = (t2.tv_sec - t1.tv_sec) + ((t2.tv_usec - t1.tv_usec) / 1e6);
  printf("gray done in %lf\n", duration);

  /* Apply blur filter with convergence value */
  gettimeofday(&t1, NULL);
  apply_blur_filter_once(image, 5, 20);
  gettimeofday(&t2, NULL);
  duration = (t2.tv_sec - t1.tv_sec) + ((t2.tv_usec - t1.tv_usec) / 1e6);
  printf("blur done in %lf\n", duration);

  /* Apply sobel filter on pixels */
  gettimeofday(&t1, NULL);
  apply_sobel_filter_once(image);
  gettimeofday(&t2, NULL);
  duration = (t2.tv_sec - t1.tv_sec) + ((t2.tv_usec - t1.tv_usec) / 1e6);
  printf("soble done in %lf\n", duration);
}

/*
 * Main entry point
 */
int main(int argc, char **argv) {
  char *input_filename;
  char *output_filename;
  char *log_filename;
  enum producer prod;  /*default, mpi, omp*/
  enum processor proc; /*default, omp*/
  animated_gif *image;
  struct timeval t1, t2;
  double duration;
  FILE *flog;

  int mpi_rank, mpi_size;
  int mpi_n_workers = 0;
  int provided;

  void (*pipe)(img *);

  /* Check command-line arguments */
  if (argc < 4) {
    fprintf(
        stderr,
        "Usage: %s input.gif output.gif log_file.log [producer] [processor]\n",
        argv[0]);
    fprintf(stderr, "producer:  default | mpi | omp\n");
    fprintf(stderr, "processor: default | omp\n");
    goto kill;
  }

  input_filename = argv[1];
  output_filename = argv[2];
  log_filename = argv[3];
  prod = parse_producer(argc <= 4 ? NULL : argv[4]);
  proc = parse_processor(argc <= 5 ? NULL : argv[5]);

  if (prod == prod_invalid) {
    fprintf(stderr, "Invalid producer parameter.\n");
    goto kill;
  }

  if (proc == proc_invalid) {
    fprintf(stderr, "Invalid processor parameter.\n");
    goto kill;
  }

  if (proc == proc_omp && prod == prod_omp) {
    fprintf(stderr,
            "Invalid combination. Cannot have omp producers and processes.\n");
    goto kill;
  }

#if SOBELF_DEBUG
  printf("\n\nARGUMENTS\n\n\n");
  printf("prod %d\n", prod);
  printf("proc %d\n", proc);
  printf("input_file %s\n", input_filename);
  printf("output_file %s\n", output_filename);
  printf("log_file %s\n", log_filename);
  printf("\n\nENDARGUMENTS\n\n\n");
#endif /* SOBELF_DEBUG */

  // Defining pipe depending on proceadure
  switch (proc) {
  case proc_omp:
    pipe = omp_pipe;
    break;
  case proc_opt:
    pipe = opt_pipe;
    break;
  default:
    // pipe = default_pipe;
    pipe = log_pipe;
    break;
  }

  if (proc == proc_omp || prod == prod_omp) // if any omp is being used
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
  else
    MPI_Init(&argc, &argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
  mpi_n_workers = mpi_size - 1;

  if (mpi_rank != ROOT) {
    mpi_worker(mpi_rank, pipe);
    MPI_Finalize();
    return 0;
  }

  /* Everything here on is for the ROOT to execute! */
  flog = fopen(log_filename, "a");
  if (flog == NULL) {
    fprintf(stderr, "Could not open log file \n");
    goto kill;
  }

  /* IMPORT Timer start */
  gettimeofday(&t1, NULL);

  /* Load file and store the pixels in array */
  image = load_pixels(input_filename);
  if (image == NULL)
    goto kill;

  /* IMPORT Timer stop */
  gettimeofday(&t2, NULL);

  duration = (t2.tv_sec - t1.tv_sec) + ((t2.tv_usec - t1.tv_usec) / 1e6);

  printf("GIF loaded from file %s with %d image(s) in %lf s\n", input_filename,
         image->n_images, duration);

  // passing images into new format
  img *images = malloc(sizeof(img) * image->n_images);
  for (int i = 0; i < image->n_images; i++) {
    images[i].width = image->width[i];
    images[i].height = image->height[i];
    images[i].id = 1;
    images[i].p = image->p[i];
  }

  /* FILTER Timer start */
  gettimeofday(&t1, NULL);

  // producing jobs according to preference
  switch (prod) {
  case prod_mpi:
    mpi_server(mpi_n_workers, image->n_images, images, ROOT);
    break;
  case prod_omp:
    omp_server(image->n_images, images, pipe);
    break;
  default:
    for (int i = 0; i < image->n_images; i++)
      pipe(images + i);
    break;
  }

  /* FILTER Timer stop */
  gettimeofday(&t2, NULL);

  duration = (t2.tv_sec - t1.tv_sec) + ((t2.tv_usec - t1.tv_usec) / 1e6);

  printf("SOBEL done in %lf s\n", duration);
  fprintf(flog, "%s; %lf\n", input_filename, duration);

  // reputting images in original format
  for (int i = 0; i < image->n_images; i++)
    image->p[i] = images[i].p;

  /* EXPORT Timer start */
  gettimeofday(&t1, NULL);

  /* Store file from array of pixels to GIF file */
  if (!store_pixels(output_filename, image)) {
    return 1;
  }

  /* EXPORT Timer stop */
  gettimeofday(&t2, NULL);

  duration = (t2.tv_sec - t1.tv_sec) + ((t2.tv_usec - t1.tv_usec) / 1e6);

  printf("Export done in %lf s in file %s\n", duration, output_filename);

  fclose(flog);

kill:;
  int k = -1;
  for (int i = 0; i < mpi_n_workers; i++)
    MPI_Send(&k, 1, MPI_INT, i + 1, 0, MPI_COMM_WORLD);

  MPI_Finalize();
  return 0;
}
