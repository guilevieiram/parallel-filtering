#pragma once
#define CONV(l, c, nb_c) (l) * (nb_c) + (c)
#include <sys/time.h>
#include "gif_lib.h"

/* Represent one pixel from the image */
typedef struct pixel {
  int r; /* Red */
  int g; /* Green */
  int b; /* Blue */
} pixel;

/* Represent one GIF image (animated or not */
typedef struct animated_gif {
  int n_images;   /* Number of images */
  int *width;     /* Width of each image */
  int *height;    /* Height of each image */
  pixel **p;      /* Pixels of each image */
  GifFileType *g; /* Internal representation.
                     DO NOT MODIFY */
} animated_gif;

typedef struct {
  int width;
  int height;
  int id;
  pixel *p;
} img;

/*
 * An image packege is an integer array representation
 * of an img to help with efficient mpi message passing
 * the array is structured in the folowing way
 * [width, height, image_id, sender_rank, p[0].r, p[0].g, p[0].b, p[1].r,
 * p[1].g, p[1].b, ...] and the total lenght is (3*height*width + 4)
 * */
typedef int *img_pkg;

void pkg2img(img_pkg pkg, img *image, int *sender_rank);
void img2pkg(img image, img_pkg pkg, int sender_rank) ;
int sizeofimg(img image);
int sizeofp(int pkg_size);

void printimg(img image); 

animated_gif *load_pixels(char *filename);
int output_modified_read_gif(char *filename, GifFileType *g);
int store_pixels(char *filename, animated_gif *image);

int test_pkg_img(void);
