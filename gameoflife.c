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

void print_step(int* grid) {
  for (int row = 0; row < GRID_WIDTH; row += DIM) {
    print_row(grid + row);
  }
}

int* get_row(int* grid, int y) {
  while (y < 0) y += DIM;
  while (y >= DIM) y -= DIM;

  return grid + (DIM * y);
}

int get_cell( int* grid, int x, int y) {
  while (x < 0) x += DIM;
  while (x >= DIM) x -= DIM;

  while (y < 0) y += DIM;
  while (y >= DIM) y -= DIM;

  return grid[DIM * y + x];
}

int is_alive( int* grid, int x, int y) {
  int was_alive = get_cell(grid, x, y);

  int neighbors[8] = {
    get_cell(grid, x - 1, y),
    get_cell(grid, x + 1, y),
    get_cell(grid, x - 1, y - 1),
    get_cell(grid, x, y - 1),
    get_cell(grid, x + 1, y - 1),
    get_cell(grid, x - 1, y + 1),
    get_cell(grid, x, y + 1),
    get_cell(grid, x + 1, y + 1)
  };
  int sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += neighbors[i];
  }

  return (sum == 3 || (was_alive && sum == 2));
}

int* step( int* grid, int start_index, int size, int ID ) {
  int* new_grid = calloc(GRID_WIDTH, sizeof(int));
  for (int i = start_index; i < start_index + size; i++) {
    int x = i % DIM;
    int y = i / DIM;
    new_grid[i] = is_alive(grid, x, y);
  }
  return new_grid;
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

  int* last = calloc(GRID_WIDTH, sizeof(int));
  memcpy(last, global_grid, GRID_WIDTH*sizeof(int));

  for ( iters = 0; iters < ITERATIONS; iters++ ) {
    int* next = step(last, start_index, split_size, ID);

    int* first_row = get_row(next, start_row);
    int* last_row = get_row(next, end_row - 1);

    MPI_Request requests[2];

    MPI_Isend(first_row, DIM, MPI_INT, prev_id, FIRST_ROW, MPI_COMM_WORLD, &requests[0]);
    MPI_Isend(last_row, DIM, MPI_INT, next_id, LAST_ROW, MPI_COMM_WORLD, &requests[1]);

    free(last);

    last = next;
    int* prev_row = get_row(last, start_row - 1);
    int* next_row = get_row(last, end_row);

    assert(MPI_Recv(prev_row, DIM, MPI_INT, prev_id, LAST_ROW, MPI_COMM_WORLD, NULL) == MPI_SUCCESS);
    assert(MPI_Recv(next_row, DIM, MPI_INT, next_id, FIRST_ROW, MPI_COMM_WORLD, NULL) == MPI_SUCCESS);

    assert(MPI_Waitall(2, requests, NULL) == MPI_SUCCESS);
  }

  if ( ID == 0 ) {
    for (int i = 1; i < num_procs; i++) {
      assert(MPI_Recv(last + i*split_size, split_size, MPI_INT, i, ALL_ROWS, MPI_COMM_WORLD, NULL) == MPI_SUCCESS);
    }
    printf ( "\nIteration %d: final grid:\n", iters );
    print_step(last);
  } else {
    assert(MPI_Send(last + start_index, split_size, MPI_INT, 0, ALL_ROWS, MPI_COMM_WORLD) == MPI_SUCCESS);
  }

  free(last);

  // TODO: Clean up memory
  MPI_Finalize(); // finalize so I can exit
}