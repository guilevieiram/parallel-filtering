#pragma once
#include "utils.h"

extern void cuda_apply_gray_filter_once(img *image);
extern void cuda_apply_blur_filter_once(img *image, int size, int threshold);
extern void cuda_apply_sobel_filter_once(img *image);