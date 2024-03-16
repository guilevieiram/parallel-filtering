#include "omp_utils.h"
#include "filters.h"

#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void omp_server(int n_images, img *images, void (*pipe)(img *)) {
#pragma omp parallel for
  for (int i = 0; i < n_images; i++) {
    pipe(images + i);
  }
}

void gray_filter_helper(pixel *p) {
  int moy;
  moy = (p->r + p->g + p->b) / 3;

  if (moy < 0)
    moy = 0;
  if (moy > 255)
    moy = 255;

  p->r = moy;
  p->g = moy;
  p->b = moy;
}

void omp_apply_gray_filter(img *image) {
  int j;
  pixel *p;
  int width, height;

  p = image->p;
  width = image->width;
  height = image->height;

#pragma omp parallel for firstprivate(width, height)
  for (j = 0; j < width * height; j++)
    gray_filter_helper(&p[j]);
}

void omp_apply_blur_filter(img *image, int size, int threshold) {
  int end = 0;
  int n_iter = 0;
  int width = image->width;
  int height = image->height;
  pixel *p = image->p;
  pixel *new = (pixel *)malloc(width * height * sizeof(pixel));
  memcpy(new, p, width * height * sizeof(pixel));

  do {
    end = 1;
    n_iter++;

    /* Apply blur on top AND bottom part of image (10%) */
#pragma GCC unroll 2
    for (int j_idx = 0; j_idx < 2; j_idx++) {

#pragma omp parallel for collapse(2)
      for (int j = size; j < height / 10 - size; j++) {
        for (int k = size; k < width - size; k++) {
          int js[] = {j, height - j - 1};
          int j_true = js[j_idx];

          int t_r = 0;
          int t_g = 0;
          int t_b = 0;

          for (int stencil_j = -size; stencil_j <= size; stencil_j++) {
            for (int stencil_k = -size; stencil_k <= size; stencil_k++) {
              t_r += p[CONV(j_true + stencil_j, k + stencil_k, width)].r;
              t_g += p[CONV(j_true + stencil_j, k + stencil_k, width)].g;
              t_b += p[CONV(j_true + stencil_j, k + stencil_k, width)].b;
            }
          }

          new[CONV(j_true, k, width)].r =
              t_r / ((2 * size + 1) * (2 * size + 1));
          new[CONV(j_true, k, width)].g =
              t_g / ((2 * size + 1) * (2 * size + 1));
          new[CONV(j_true, k, width)].b =
              t_b / ((2 * size + 1) * (2 * size + 1));

          // calculate diffs direclty
          float diff_r =
              (new[CONV(j_true, k, width)].r - p[CONV(j_true, k, width)].r);
          float diff_g =
              (new[CONV(j_true, k, width)].g - p[CONV(j_true, k, width)].g);
          float diff_b =
              (new[CONV(j_true, k, width)].b - p[CONV(j_true, k, width)].b);

          if (diff_r > threshold || -diff_r > threshold || diff_g > threshold ||
              -diff_g > threshold || diff_b > threshold ||
              -diff_b > threshold) {
            end = 0;
          }
        }
      }
    }
    pixel *tmp = p;
    p = new;
    new = tmp;
  } while (threshold > 0 && !end);

#if SOBELF_DEBUG
  printf("BLUR: number of iterations for image %d\n", n_iter);
#endif

  free(new);
  image->p = p;
}

void omp_apply_sobel_filter(img *image) {
  pixel *p = image->p;
  int width = image->width;
  int height = image->height;

  pixel *sobel = (pixel *)malloc(width * height * sizeof(pixel));
  memcpy(sobel, p,width * height * sizeof(pixel));

#pragma omp parallel for collapse(2)
  for (int j = 1; j < height - 1; j++) {
    for (int k = 1; k < width - 1; k++) {
      int pixel_blue_no, pixel_blue_n, pixel_blue_ne;
      int pixel_blue_so, pixel_blue_s, pixel_blue_se;
      int pixel_blue_o, pixel_blue_e;

      float deltaX_blue;
      float deltaY_blue;
      float val_blue;

      pixel_blue_no = p[CONV(j - 1, k - 1, width)].b;
      pixel_blue_n = p[CONV(j - 1, k, width)].b;
      pixel_blue_ne = p[CONV(j - 1, k + 1, width)].b;
      pixel_blue_so = p[CONV(j + 1, k - 1, width)].b;
      pixel_blue_s = p[CONV(j + 1, k, width)].b;
      pixel_blue_se = p[CONV(j + 1, k + 1, width)].b;
      pixel_blue_o = p[CONV(j, k - 1, width)].b;
      pixel_blue_e = p[CONV(j, k + 1, width)].b;

      deltaX_blue = -pixel_blue_no + pixel_blue_ne - 2 * pixel_blue_o +
                    2 * pixel_blue_e - pixel_blue_so + pixel_blue_se;

      deltaY_blue = pixel_blue_se + 2 * pixel_blue_s + pixel_blue_so -
                    pixel_blue_ne - 2 * pixel_blue_n - pixel_blue_no;

      val_blue =
          sqrt(deltaX_blue * deltaX_blue + deltaY_blue * deltaY_blue) / 4;

      if (val_blue > 50) {
        sobel[CONV(j, k, width)].r = 255;
        sobel[CONV(j, k, width)].g = 255;
        sobel[CONV(j, k, width)].b = 255;
      } else {
        sobel[CONV(j, k, width)].r = 0;
        sobel[CONV(j, k, width)].g = 0;
        sobel[CONV(j, k, width)].b = 0;
      }
    }
  }

  free(p);
  image->p = sobel;
}
