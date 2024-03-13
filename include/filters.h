#pragma once
#include "utils.h"
#define CONV(l, c, nb_c) (l) * (nb_c) + (c)

void apply_gray_filter_once(img *image);
void apply_gray_line_once(img *image);
void apply_blur_filter_once(img *image, int size, int threshold);
void apply_blur_filter_once_opt(img *image, int size, int threshold);
void apply_sobel_filter_once(img *image);
void apply_sobel_filter_once_opt(img *image);
