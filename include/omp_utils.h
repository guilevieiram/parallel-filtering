#pragma once
#include "utils.h"

void omp_server(int n_images, img *images, void (*pipe)(img *));
void omp_apply_gray_filter(img *image);
void omp_apply_blur_filter(img *image, int size, int threshold);
void omp_apply_sobel_filter(img *image);
