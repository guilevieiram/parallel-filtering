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

#include "cuda_filters.h"
#include "filters.h"
#include "utils.h"

#include "mpi_utils.h"
#include "omp_utils.h"

#define SOBELF_DEBUG 0
#define ROOT 0
#define NPIXELS_THRESHOLD 1000000

enum producer { prod_invalid, prod_def, prod_mpi, prod_omp };

enum processor { proc_invalid, proc_def, proc_opt, proc_omp, proc_cuda };

char *get_prod_name(enum producer p) {
  switch (p) {
  case prod_def:
    return "default";
  case prod_omp:
    return "OMP";
  case prod_mpi:
    return "MPI";
  default:
    return "";
  }
}

char *get_proc_name(enum processor p) {
  switch (p) {
  case proc_def:
    return "default";
  case proc_omp:
    return "OMP";
  case proc_cuda:
    return "CUDA";
  case proc_opt:
    return "optimized default";
  default:
    return "";
  }
}

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
  else if (!strcmp(str, "cuda"))
    return proc_cuda;
  else
    return proc_invalid;
}

void decide_parameters(img *images, enum processor *out_processor,
                       enum producer *out_producer) {

  int n_pixels = images[0].width * images[0].height;
  int n_threads = omp_get_max_threads();
  *out_producer = prod_def;

  if (is_cuda_available() && n_pixels > NPIXELS_THRESHOLD)
    *out_processor = proc_cuda;
  else if (n_threads > 1)
    *out_processor = proc_omp;
  else
    *out_processor = proc_def;
}

void omp_pipe(img *image) {
#if SOBELF_DEBUG
  printf("Available threads in pipe: %d \n", omp_get_max_threads());
#endif

  omp_apply_gray_filter(image);

  /* Apply blur filter with convergence value */
  omp_apply_blur_filter(image, 5, 20);

  /* Apply sobel filter on pixels */
  omp_apply_sobel_filter(image);
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
  enum processor proc; /*default, omp, cuda*/
  animated_gif *image;
  struct timeval t1, t2;
  double duration;
  FILE *flog;

  int mpi_rank, mpi_size;
  int mpi_n_workers = 0;
  int provided;

  void (*pipe)(img *);

  /* Check command-line arguments */
  if (argc != 4 && argc != 6) {
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

  MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
  mpi_n_workers = mpi_size - 1;

  /* IMPORT Timer start */
  gettimeofday(&t1, NULL);

  /* Load file and store the pixels in array */
  image = load_pixels(input_filename);
  if (image == NULL)
    goto kill;

  /* IMPORT Timer stop */
  gettimeofday(&t2, NULL);

  duration = (t2.tv_sec - t1.tv_sec) + ((t2.tv_usec - t1.tv_usec) / 1e6);

  if (mpi_rank == ROOT)
    printf("GIF loaded from file %s with %d image(s) in %lf s\n",
           input_filename, image->n_images, duration);

  // passing images into new format
  img *images = malloc(sizeof(img) * image->n_images);
  for (int i = 0; i < image->n_images; i++) {
    images[i].width = image->width[i];
    images[i].height = image->height[i];
    images[i].id = 1;
    images[i].p = image->p[i];
  }

  if (argc == 4) {
    decide_parameters(images, &proc, &prod);
  }
  if (argc == 6) {
    prod = parse_producer(argv[4]);
    proc = parse_processor(argv[5]);
  }

  // print the current configuration
  printf("Running with configuration\n");
  printf("\tProducer: %s\n", get_prod_name(prod));
  printf("\tProcessor: %s\n", get_proc_name(proc));

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

  // Defining pipe depending on proceadure
  switch (proc) {
  case proc_omp:
    pipe = omp_pipe;
    break;
  case proc_opt:
    pipe = opt_pipe;
    break;
  case proc_cuda:
    pipe = cuda_pipe;
    break;
  default:
    pipe = default_pipe;
    break;
  }

  if (mpi_n_workers <= 0 && prod == prod_mpi) {
    fprintf(stderr, "Invalid combination. Cannot have mpi producers with only "
                    "one available rank.\n");
    goto kill;
  }

  if (mpi_rank != ROOT) {
    mpi_worker(mpi_rank, pipe);
    MPI_Finalize();
    return 0;
  }

  /* Everything here on is for the ROOT to execute! */
  flog = fopen(log_filename, "a");
  if (flog == NULL) {
    fprintf(stderr, "Could not open log file (%s)\n", log_filename);
    goto kill;
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
