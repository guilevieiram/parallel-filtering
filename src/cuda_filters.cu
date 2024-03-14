#include "utils.h"

#include <cuda_runtime.h>

#define SOBEL_R 1

#define TILE_HEIGHT 16
#define TILE_WIDTH 16

#define BLOCK_WIDTH (TILE_WIDTH + (2 * SOBEL_R))
#define BLOCK_HEIGHT (TILE_HEIGHT + (2 * SOBEL_R))

__global__ void gray_filter_kernel(pixel *p, unsigned size)
{
  unsigned i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < size)
  {
    int moy = p[i].r + p[i].g + p[i].b;
    moy = 255 * (moy > 3 * 255);

    p[i].r = moy;
    p[i].g = moy;
    p[i].b = moy;
  }
}

__global__ void blur_filter_kernel(pixel *p, pixel *new_p, int width, int height, int size, int threshold)
{
}

// Inspired by Nvidia CUDA samples
__global__ void sobel_filter_kernel(pixel *p, pixel *new_p, int width, int height)
{
  __shared__ int smem[BLOCK_HEIGHT * BLOCK_WIDTH];

  int x = blockIdx.x * TILE_WIDTH + threadIdx.x - SOBEL_R;
  int y = blockIdx.y * TILE_HEIGHT + threadIdx.y - SOBEL_R;

  unsigned i = y * width + x;
  unsigned smem_i = threadIdx.y * BLOCK_WIDTH + threadIdx.x;

  if (x >= 0 && x < width && y >= 0 && y < height)
  {
    smem[smem_i] = p[i].b;
  }

  __syncthreads();

  if (threadIdx.x >= SOBEL_R && threadIdx.x < TILE_WIDTH + SOBEL_R &&
      threadIdx.y >= SOBEL_R && threadIdx.y < TILE_HEIGHT + SOBEL_R &&
      x >= 1 && x < width - 1 && y >= 1 && y < height - 1)
  {
    float delta_x = 2 * (smem[smem_i + 1] - smem[smem_i - 1]) +
                    (smem[smem_i - BLOCK_WIDTH + 1] - smem[smem_i - BLOCK_WIDTH - 1]) +
                    (smem[smem_i + BLOCK_WIDTH + 1] - smem[smem_i + BLOCK_WIDTH - 1]);

    float delta_y = 2 * (smem[smem_i + BLOCK_WIDTH] - smem[smem_i - BLOCK_WIDTH]) +
                    (smem[smem_i + BLOCK_WIDTH - 1] - smem[smem_i - BLOCK_WIDTH - 1]) +
                    (smem[smem_i + BLOCK_WIDTH + 1] - smem[smem_i - BLOCK_WIDTH + 1]);

    int new_val = sqrt(delta_x * delta_x + delta_y * delta_y) / 4;

    new_p[i].r = (new_val > 50) * 255;
    new_p[i].g = (new_val > 50) * 255;
    new_p[i].b = (new_val > 50) * 255;
  }
}

extern "C" void cuda_apply_gray_filter_once(img *image_d)
{
  const size_t block_size = 256;
  const size_t num_blocks = (image_d->width * image_d->height + block_size - 1) / block_size;
  gray_filter_kernel<<<num_blocks, block_size>>>(image_d->p, image_d->width * image_d->height);
}

extern "C" void cuda_apply_blur_filter_once(img *image, int size, int threshold)
{
}

extern "C" void cuda_apply_sobel_filter_once(img *image)
{
  pixel *new_p_d = nullptr;
  cudaMalloc(&new_p_d, image->width * image->height * sizeof(pixel));

  const dim3 block_size(BLOCK_WIDTH, BLOCK_HEIGHT);
  const dim3 num_blocks((image->width + TILE_WIDTH - 1) / TILE_WIDTH, (image->height + TILE_HEIGHT - 1) / TILE_HEIGHT);
  sobel_filter_kernel<<<num_blocks, block_size>>>(image->p, new_p_d, image->width, image->height);

  cudaFree(image->p);
  image->p = new_p_d;  
}

extern "C" void cuda_pipe(img *image)
{
  /* Allocate memory for the image on device*/
  img image_d = *image;
  cudaMalloc(&image_d.p, image_d.width * image_d.height * sizeof(pixel));
  cudaMemcpy(image_d.p, image->p, image->width * image->height * sizeof(pixel), cudaMemcpyHostToDevice);

  /* Convert the pixels into grayscale */
  cuda_apply_gray_filter_once(&image_d);

  /* Apply blur filter with convergence value */
  cuda_apply_blur_filter_once(&image_d, 5, 20);

  /* Apply sobel filter on pixels */
  cuda_apply_sobel_filter_once(&image_d);

  /* Copy the pixels back to the host and frees memmory */
  cudaDeviceSynchronize();
  cudaMemcpy(image->p, image_d.p, image->width * image->height * sizeof(pixel), cudaMemcpyDeviceToHost);
  cudaFree(image_d.p);
}