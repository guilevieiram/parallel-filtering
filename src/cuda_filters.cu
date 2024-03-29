#include "utils.h"

#include <cuda_runtime.h>
#include <stdio.h>

#define SOBEL_R 1

#define THREADS_PER_BLOCK 256

#define TILE_HEIGHT 16
#define TILE_WIDTH 16

#define BLOCK_WIDTH (TILE_WIDTH + (2 * SOBEL_R))
#define BLOCK_HEIGHT (TILE_HEIGHT + (2 * SOBEL_R))

__global__ void gray_filter_kernel(pixel *p, unsigned size) {
  unsigned i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < size) {
    int moy = (p[i].r + p[i].g + p[i].b) / 3;
    moy = max(0, min(255, moy));

    p[i].r = moy;
    p[i].g = moy;
    p[i].b = moy;
  }
}

// Inspired by Nvidia CUDA samples
__global__ void sobel_filter_kernel(pixel *p, pixel *new_p, int width,
                                    int height) {
  __shared__ int smem[BLOCK_HEIGHT * BLOCK_WIDTH];

  int x = blockIdx.x * TILE_WIDTH + threadIdx.x - SOBEL_R;
  int y = blockIdx.y * TILE_HEIGHT + threadIdx.y - SOBEL_R;

  unsigned i = y * width + x;
  unsigned smem_i = threadIdx.y * BLOCK_WIDTH + threadIdx.x;

  if (x >= 0 && x < width && y >= 0 && y < height) {
    smem[smem_i] = p[i].b;
  }

  __syncthreads();

  if (threadIdx.x >= SOBEL_R && threadIdx.x < TILE_WIDTH + SOBEL_R &&
      threadIdx.y >= SOBEL_R && threadIdx.y < TILE_HEIGHT + SOBEL_R && x >= 1 &&
      x < width - 1 && y >= 1 && y < height - 1) {
    float delta_x =
        2 * (smem[smem_i + 1] - smem[smem_i - 1]) +
        (smem[smem_i - BLOCK_WIDTH + 1] - smem[smem_i - BLOCK_WIDTH - 1]) +
        (smem[smem_i + BLOCK_WIDTH + 1] - smem[smem_i + BLOCK_WIDTH - 1]);

    float delta_y =
        2 * (smem[smem_i + BLOCK_WIDTH] - smem[smem_i - BLOCK_WIDTH]) +
        (smem[smem_i + BLOCK_WIDTH - 1] - smem[smem_i - BLOCK_WIDTH - 1]) +
        (smem[smem_i + BLOCK_WIDTH + 1] - smem[smem_i - BLOCK_WIDTH + 1]);

    float new_val = sqrt(delta_x * delta_x + delta_y * delta_y) / 4;

    new_p[i].r = (new_val > 50) * 255;
    new_p[i].g = (new_val > 50) * 255;
    new_p[i].b = (new_val > 50) * 255;
  }
}

__global__ void horizontal_pass(pixel *p, pixel *p_out, int width, int height,
                                int size) {
  int thread_idx = blockIdx.x * blockDim.x + threadIdx.x;

  if (thread_idx >= height) {
    return;
  }

  int sum = 0;
  int idx = thread_idx * width;
  for (int i = 0; i < 2 * size + 1; i++) {
    sum += p[idx + i].r;
  }

  for (int i = 0; i < width - 2 * size; i++) {
    p_out[idx + size + i].r = sum;
    p_out[idx + size + i].g = sum;
    p_out[idx + size + i].b = sum;

    sum += p[idx + i + 2 * size + 1].r;
    sum -= p[idx + i].r;
  }
}

__global__ void vertical_pass(pixel *p, pixel *p_out, int width, int height,
                              int size) {
  int sum = 0;
  int idx = blockIdx.x * blockDim.x + threadIdx.x + size;

  if (idx >= width - size) {
    return;
  }

  for (int i = 0; i < 2 * size + 1; i++) {
    sum += p[idx + width * i].r;
  }

  for (int i = 0; i < height - 2 * size; i++) {
    p_out[idx + width * (i + size)].r = sum;
    p_out[idx + width * (i + size)].g = sum;
    p_out[idx + width * (i + size)].b = sum;

    sum += p[idx + width * (i + 2 * size + 1)].r;
    sum -= p[idx + width * i].r;
  }
}

__global__ void normalize_pixel_values(pixel *img, int width, int height,
                                       int size, int area) {
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;

  if (x >= width - size || y >= height - size || x < size || y < size) {
    return;
  }

  int idx = y * width + x;
  img[idx].r /= area;
  img[idx].g /= area;
  img[idx].b /= area;
}

// Inspired by cuda samples, modified for simplicity and our use case
__global__ void reduce(pixel *orig_p, pixel *mod_p, int *out_flag, int n,
                       int threshold) {
  __shared__ int sdata[THREADS_PER_BLOCK];
  unsigned int tid = threadIdx.x;
  unsigned int i = blockIdx.x * blockDim.x + threadIdx.x;

  if (i < n && (orig_p[i].r - mod_p[i].r > threshold ||
                mod_p[i].r - orig_p[i].r > threshold)) {
    sdata[tid] = 1;
  } else {
    sdata[tid] = 0;
  }
  __syncthreads();

  for (unsigned int s = blockDim.x / 2; s > 0; s >>= 1) {
    if (tid < s) {
      sdata[tid] |= sdata[tid + s];
    }
    __syncthreads();
  }

  if (tid == 0 && sdata[0]) {
    atomicExch(out_flag, sdata[0]);
  }
}

extern "C" void cuda_apply_gray_filter_once(img *image_d) {
  const size_t block_size = THREADS_PER_BLOCK;
  const size_t num_blocks =
      (image_d->width * image_d->height + block_size - 1) / block_size;
  gray_filter_kernel<<<num_blocks, block_size>>>(
      image_d->p, image_d->width * image_d->height);
}

extern "C" void cuda_apply_blur_filter_once(img *image, int size,
                                            int threshold) {
  pixel *temp_p_d = nullptr;
  cudaMalloc(&temp_p_d, image->width * image->height * sizeof(pixel));
  pixel *new_p_d = nullptr;
  cudaMalloc(&new_p_d, image->width * image->height * sizeof(pixel));

  int *cont_flag_d = nullptr;
  cudaMalloc(&cont_flag_d, sizeof(int));

  int cont_flag = 0;
  int n_iter = 0;
  do {
    cudaMemcpy(new_p_d, image->p, image->width * image->height * sizeof(pixel),
               cudaMemcpyDeviceToDevice);
    cudaMemcpy(temp_p_d, image->p, image->width * image->height * sizeof(pixel),
               cudaMemcpyDeviceToDevice);

    // second dimension is used to blur either the bottom or top of the image
    // not yet implemented
    const dim3 block_size(THREADS_PER_BLOCK, 1);
    const dim3 num_hor_blocks((image->height + block_size.x - 1) / block_size.x,
                              1);
    const dim3 num_vert_blocks((image->width + block_size.x - 1) / block_size.x,
                               1);

    const dim3 block_size_norm(BLOCK_WIDTH, BLOCK_HEIGHT);
    const dim3 num_norm_blocks(
        (image->width + block_size_norm.x - 1) / block_size_norm.x,
        (image->height + block_size_norm.y - 1) / block_size_norm.y);

    horizontal_pass<<<num_hor_blocks, block_size>>>(
        image->p, temp_p_d, image->width, image->height / 10, size);
    vertical_pass<<<num_vert_blocks, block_size>>>(
        temp_p_d, new_p_d, image->width, image->height / 10, size);
    normalize_pixel_values<<<num_norm_blocks, block_size_norm>>>(
        new_p_d, image->width, image->height / 10, size,
        (2 * size + 1) * (2 * size + 1));

    // TODO: implement this using block y dimension
    const int lower_bar_height = (image->height + 9) / 10;
    const int offset = image->width * (image->height - lower_bar_height);
    horizontal_pass<<<num_hor_blocks, block_size>>>(
        image->p + offset, temp_p_d + offset, image->width, lower_bar_height,
        size);
    vertical_pass<<<num_vert_blocks, block_size>>>(
        temp_p_d + offset, new_p_d + offset, image->width, lower_bar_height,
        size);
    normalize_pixel_values<<<num_norm_blocks, block_size_norm>>>(
        new_p_d + offset, image->width, lower_bar_height, size,
        (2 * size + 1) * (2 * size + 1));

    int num_reduction_blocks =
        (image->width * image->height + block_size.x - 1) / block_size.x;
    cudaMemset(cont_flag_d, 0, sizeof(int));
    reduce<<<num_reduction_blocks, block_size>>>(image->p, new_p_d, cont_flag_d,
                                                 image->width * image->height,
                                                 threshold);
    cudaMemcpy(&cont_flag, cont_flag_d, sizeof(int), cudaMemcpyDeviceToHost);

    pixel *temp = image->p;
    image->p = new_p_d;
    new_p_d = temp;

    n_iter++;
  } while (threshold > 0 && cont_flag);
#if SOBELF_DEBUG
  printf("BLUR: number of iterations for image %d\n", n_iter);
#endif

  cudaFree(new_p_d);
  cudaFree(temp_p_d);
  cudaFree(cont_flag_d);
}

extern "C" void cuda_apply_sobel_filter_once(img *image) {
  pixel *new_p_d = nullptr;
  cudaMalloc(&new_p_d, image->width * image->height * sizeof(pixel));
  // TODO: Fix, this works but is not efficient. I tried doing it in the kernel
  // but it didn't work
  cudaMemcpy(new_p_d, image->p, image->width * image->height * sizeof(pixel),
             cudaMemcpyDeviceToDevice);

  const dim3 block_size(BLOCK_WIDTH, BLOCK_HEIGHT);
  const dim3 num_blocks((image->width + TILE_WIDTH - 1) / TILE_WIDTH,
                        (image->height + TILE_HEIGHT - 1) / TILE_HEIGHT);
  sobel_filter_kernel<<<num_blocks, block_size>>>(image->p, new_p_d,
                                                  image->width, image->height);

  cudaFree(image->p);
  image->p = new_p_d;
}

extern "C" void cuda_pipe(img *image) {
  /* Allocate memory for the image on device*/
  img image_d = *image;
  cudaMalloc(&image_d.p, image_d.width * image_d.height * sizeof(pixel));
  cudaMemcpy(image_d.p, image->p, image->width * image->height * sizeof(pixel),
             cudaMemcpyHostToDevice);

  /* Convert the pixels into grayscale */
  cuda_apply_gray_filter_once(&image_d);

  /* Apply blur filter with convergence value */
  cuda_apply_blur_filter_once(&image_d, 5, 20);

  /* Apply sobel filter on pixels */
  cuda_apply_sobel_filter_once(&image_d);

  /* Copy the pixels back to the host and frees memmory */
  cudaDeviceSynchronize();
  cudaMemcpy(image->p, image_d.p, image->width * image->height * sizeof(pixel),
             cudaMemcpyDeviceToHost);
  cudaFree(image_d.p);
}

extern "C" int is_cuda_available(void){
    int deviceCount = 0;
    cudaGetDeviceCount(&deviceCount);
    return deviceCount > 0;
}
