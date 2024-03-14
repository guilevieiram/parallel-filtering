#include "filters.h"
#include "utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void apply_gray_filter_once(img *image) {
  int j;
  pixel *p;
  int width, height;

  p = image->p;
  width = image->width;
  height = image->height;

  for (j = 0; j < width * height; j++) {
    int moy;

    moy = (p[j].r + p[j].g + p[j].b) / 3;
    if (moy < 0)
      moy = 0;
    if (moy > 255)
      moy = 255;

    p[j].r = moy;
    p[j].g = moy;
    p[j].b = moy;
  }
}

void apply_blur_filter_once(img *image, int size, int threshold) {
  int j, k;
  int width, height;
  int end = 0;
  int n_iter = 0;

  pixel *p;
  pixel *new;

  /* Process all images */
  n_iter = 0;
  p = image->p;
  width = image->width;
  height = image->height;

  /* Allocate array of new pixels */
  new = (pixel *)malloc(width * height * sizeof(pixel));

  /* Perform at least one blur iteration */

  do {
    end = 1;
    n_iter++;

    for (j = 0; j < height - 1; j++) {
      for (k = 0; k < width - 1; k++) {
        new[CONV(j, k, width)].r = p[CONV(j, k, width)].r;
        new[CONV(j, k, width)].g = p[CONV(j, k, width)].g;
        new[CONV(j, k, width)].b = p[CONV(j, k, width)].b;
      }
    }

    /* Apply blur on top part of image (10%) */
    for (j = size; j < height / 10 - size; j++) {
      for (k = size; k < width - size; k++) {
        int stencil_j, stencil_k;
        int t_r = 0;
        int t_g = 0;
        int t_b = 0;

        for (stencil_j = -size; stencil_j <= size; stencil_j++) {
          for (stencil_k = -size; stencil_k <= size; stencil_k++) {
            t_r += p[CONV(j + stencil_j, k + stencil_k, width)].r;
            t_g += p[CONV(j + stencil_j, k + stencil_k, width)].g;
            t_b += p[CONV(j + stencil_j, k + stencil_k, width)].b;
          }
        }

        new[CONV(j, k, width)].r = t_r / ((2 * size + 1) * (2 * size + 1));
        new[CONV(j, k, width)].g = t_g / ((2 * size + 1) * (2 * size + 1));
        new[CONV(j, k, width)].b = t_b / ((2 * size + 1) * (2 * size + 1));
      }
    }

    /* Copy the middle part of the image */
    for (j = height / 10 - size; j < height * 0.9 + size; j++) {
      for (k = size; k < width - size; k++) {
        new[CONV(j, k, width)].r = p[CONV(j, k, width)].r;
        new[CONV(j, k, width)].g = p[CONV(j, k, width)].g;
        new[CONV(j, k, width)].b = p[CONV(j, k, width)].b;
      }
    }

    /* Apply blur on the bottom part of the image (10%) */
    for (j = height * 0.9 + size; j < height - size; j++) {
      for (k = size; k < width - size; k++) {
        int stencil_j, stencil_k;
        int t_r = 0;
        int t_g = 0;
        int t_b = 0;

        for (stencil_j = -size; stencil_j <= size; stencil_j++) {
          for (stencil_k = -size; stencil_k <= size; stencil_k++) {
            t_r += p[CONV(j + stencil_j, k + stencil_k, width)].r;
            t_g += p[CONV(j + stencil_j, k + stencil_k, width)].g;
            t_b += p[CONV(j + stencil_j, k + stencil_k, width)].b;
          }
        }

        new[CONV(j, k, width)].r = t_r / ((2 * size + 1) * (2 * size + 1));
        new[CONV(j, k, width)].g = t_g / ((2 * size + 1) * (2 * size + 1));
        new[CONV(j, k, width)].b = t_b / ((2 * size + 1) * (2 * size + 1));
      }
    }

    for (j = 1; j < height - 1; j++) {
      for (k = 1; k < width - 1; k++) {

        float diff_r;
        float diff_g;
        float diff_b;

        diff_r = (new[CONV(j, k, width)].r - p[CONV(j, k, width)].r);
        diff_g = (new[CONV(j, k, width)].g - p[CONV(j, k, width)].g);
        diff_b = (new[CONV(j, k, width)].b - p[CONV(j, k, width)].b);

        if (diff_r > threshold || -diff_r > threshold || diff_g > threshold ||
            -diff_g > threshold || diff_b > threshold || -diff_b > threshold) {
          end = 0;
        }

        p[CONV(j, k, width)].r = new[CONV(j, k, width)].r;
        p[CONV(j, k, width)].g = new[CONV(j, k, width)].g;
        p[CONV(j, k, width)].b = new[CONV(j, k, width)].b;
      }
    }

  } while (threshold > 0 && !end);

#if SOBELF_DEBUG
  printf("BLUR: number of iterations for image %d\n", n_iter);
#endif

  free(new);
}



void apply_sobel_filter_once(img *image) {
  int j, k;
  int width, height;

  pixel *p;

  p = image->p;
  width = image->width;
  height = image->height;

  pixel *sobel;

  sobel = (pixel *)malloc(width * height * sizeof(pixel));

  for (j = 1; j < height - 1; j++) {
    for (k = 1; k < width - 1; k++) {
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
  for (j = 1; j < height - 1; j++) {
    for (k = 1; k < width - 1; k++) {
      p[CONV(j, k, width)].r = sobel[CONV(j, k, width)].r;
      p[CONV(j, k, width)].g = sobel[CONV(j, k, width)].g;
      p[CONV(j, k, width)].b = sobel[CONV(j, k, width)].b;
    }
  }

  free(sobel);
}

void apply_blur_filter_once_opt(img *image, const int size,
                                const int threshold) {
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
#pragma unroll
    for (int j_idx = 0; j_idx < 2; j_idx++) {
      for (int j = size; j < height / 10 - size; j++) {

        int js[] = {j, height - j - 1};

        for (int k = size; k < width - size; k++) {
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

void apply_sobel_filter_once_opt(img *image) {
  int j, k;
  int width, height;

  pixel *p;

  p = image->p;
  width = image->width;
  height = image->height;

  pixel *sobel;

  sobel = (pixel *)malloc(width * height * sizeof(pixel));

  for (j = 1; j < height - 1; j++) {
    for (k = 1; k < width - 1; k++) {
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

