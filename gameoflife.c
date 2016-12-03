// gameoflife.c
// Name: YOUR NAME HERE
// JHED: YOUR JHED HERE

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "mpi.h"

#define ITERATIONS 64
#define GRID_WIDTH  256
#define DIM  16     // assume a square grid

#define FIRST_ROW 0
#define LAST_ROW 1
#define ALL_ROWS 2

void print_row(int* row) {
  for (int i = 0; i < DIM; i++) {
    printf("%d  ", row[i]);
  }
  printf("\n");
}

int* get_row(int* grid, int y) {
  return grid + (DIM * y);
}

void print_step(int* grid, int rows) {
  for (int row = 0; row < rows; row ++) {
    print_row(get_row(grid, row));
  }
}

int get_cell( int* grid, int x, int y, int rows) {
  while (x < 0) x += DIM;
  while (x >= DIM) x -= DIM;

  return grid[DIM * y + x];
}

int is_alive( int* grid, int x, int y, int rows) {
  int was_alive = get_cell(grid, x, y, rows);

  int neighbors[8] = {
    get_cell(grid, x - 1, y, rows),
    get_cell(grid, x + 1, y, rows),
    get_cell(grid, x - 1, y - 1, rows),
    get_cell(grid, x, y - 1, rows),
    get_cell(grid, x + 1, y - 1, rows),
    get_cell(grid, x - 1, y + 1, rows),
    get_cell(grid, x, y + 1, rows),
    get_cell(grid, x + 1, y + 1, rows)
  };
  int sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += neighbors[i];
  }

  return (sum == 3 || (was_alive && sum == 2));
}

void step(int* last, int* curr, int split_size, int rows) {
  for (int i = DIM; i < split_size + DIM; i++) {
    int x = i % DIM;
    int y = i / DIM;
    curr[i] = is_alive(last, x, y, rows);
  }
}

int main ( int argc, char** argv ) {

  int global_grid[ 256 ] =
  { 0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,
    0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

  // MPI Standard variable
  int num_procs;
  int ID, j;
  int iters = 0;

  // Messaging variables
  MPI_Status stat;
  // TODO add other variables as necessary

  // MPI Setup
  if ( MPI_Init( &argc, &argv ) != MPI_SUCCESS )
  {
    printf ( "MPI_Init error\n" );
  }

  MPI_Comm_size ( MPI_COMM_WORLD, &num_procs ); // Set the num_procs
  MPI_Comm_rank ( MPI_COMM_WORLD, &ID );

  int split_size = GRID_WIDTH / num_procs;

  int start_index = GRID_WIDTH / num_procs * ID;
  int end_index =  GRID_WIDTH / num_procs * (ID + 1);

  int start_row = start_index / DIM;
  int end_row = end_index / DIM;

  int prev_id = (num_procs + ID - 1) % num_procs;
  int next_id = (ID + 1) % num_procs;

  assert ( DIM % num_procs == 0 );

  int* buffers[2];
  buffers[0] = calloc(split_size + (2 * DIM), sizeof(int));
  buffers[1] = calloc(split_size + (2 * DIM), sizeof(int));

  int current_buf = 1;
  memcpy(get_row(buffers[0], 1), get_row(global_grid, start_row), sizeof(int) * split_size);

  for ( iters = 0; iters < ITERATIONS; iters++ ) {
    int* last = buffers[(current_buf + 1) % 2];
    int* curr = buffers[current_buf];

    MPI_Request requests[2];

    MPI_Isend(get_row(last, 1), DIM, MPI_INT, prev_id, FIRST_ROW, MPI_COMM_WORLD, &requests[0]);
    MPI_Isend(get_row(last, end_row - start_row), DIM, MPI_INT, next_id, LAST_ROW, MPI_COMM_WORLD, &requests[1]);

    assert(MPI_Recv(get_row(last, 0), DIM, MPI_INT, prev_id, LAST_ROW, MPI_COMM_WORLD, NULL) == MPI_SUCCESS);
    assert(MPI_Recv(get_row(last, end_row - start_row + 1), DIM, MPI_INT, next_id, FIRST_ROW, MPI_COMM_WORLD, NULL) == MPI_SUCCESS);

    assert(MPI_Waitall(2, requests, NULL) == MPI_SUCCESS);

    step(last, curr, split_size, end_row - start_row);
    current_buf = (current_buf + 1) % 2;
  }

  int* final = buffers[(current_buf + 1) % 2] + DIM;
  if ( ID == 0 ) {
    memcpy(global_grid, final, split_size);

    for (int i = 1; i < num_procs; i++) {
      assert(MPI_Recv(global_grid + i*split_size, split_size, MPI_INT, i, ALL_ROWS, MPI_COMM_WORLD, NULL) == MPI_SUCCESS);
    }
    printf ( "\nIteration %d: final grid:\n", iters );
    print_step(global_grid, DIM);
  } else {
    assert(MPI_Send(final, split_size, MPI_INT, 0, ALL_ROWS, MPI_COMM_WORLD) == MPI_SUCCESS);
  }

  free(buffers[0]);
  free(buffers[1]);

  // TODO: Clean up memory
  MPI_Finalize(); // finalize so I can exit
}