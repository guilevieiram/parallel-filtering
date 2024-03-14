SRC_DIR=src
HEADER_DIR=include
OBJ_DIR=obj

CC=mpicc
CUDA_CC=nvcc
CFLAGS=-O3 -I$(HEADER_DIR) -Wall -Wextra -Wpedantic 
OMP_FLAGS=-fopenmp
CUDA_FLAGS=-I/usr/local/cuda/include -L/usr/local/cuda/lib64 -lcudart -lcuda
LDFLAGS=-lm

SRC= dgif_lib.c \
	egif_lib.c \
	gif_err.c \
	gif_font.c \
	gif_hash.c \
	gifalloc.c \
	openbsd-reallocarray.c \
	quantize.c \
	mpi_utils.c \
	omp_utils.c \
	filters.c \
	utils.c \
	main.c

CUDA_SRC= cuda_filters.cu

OBJ= $(OBJ_DIR)/dgif_lib.o \
	$(OBJ_DIR)/egif_lib.o \
	$(OBJ_DIR)/gif_err.o \
	$(OBJ_DIR)/gif_font.o \
	$(OBJ_DIR)/gif_hash.o \
	$(OBJ_DIR)/gifalloc.o \
	$(OBJ_DIR)/openbsd-reallocarray.o \
	$(OBJ_DIR)/quantize.o \
	$(OBJ_DIR)/mpi_utils.o \
	$(OBJ_DIR)/omp_utils.o \
	$(OBJ_DIR)/filters.o \
	$(OBJ_DIR)/utils.o \
	$(OBJ_DIR)/main.o \
	$(OBJ_DIR)/cuda_filters.o

all: $(OBJ_DIR) sobelf

$(OBJ_DIR):
	mkdir $(OBJ_DIR)

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(OMP_FLAGS) -c -o $@ $^ 

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cu
	$(CUDA_CC) -Iinclude -c -o $@ $^

sobelf:$(OBJ)
	$(CC) $(CFLAGS) $(OMP_FLAGS) $(CUDA_FLAGS) -o $@ $^ $(LDFLAGS) 

clean:
	rm -f sobelf $(OBJ) 
