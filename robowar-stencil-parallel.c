#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <omp.h>

#define GIF_IMPLEMENTATION
#include "gif.h"

#define SIZE 120
#define SCALE 6
#define NSTATES 2
#define FRAME_DELAY 60

/* Cell contents:
   'A' -> player of squad A alive
   'B' -> player of squad B alive
   'D' -> dead
    0  -> empty
*/

const unsigned int LEFT = 1;
const unsigned int RIGHT = SIZE;
const unsigned int TOP = 1;
const unsigned int BOTTOM = SIZE;

/* double buffer with 1-cell halo on each side */
unsigned char grid[NSTATES][SIZE + 2][SIZE + 2];
int cur_step = 0;

unsigned char spawn_pixel(float p1, float p2, unsigned int *seed);
void init(float p1, float p2);
unsigned int count_active_neighbors(unsigned int k, unsigned int m);
unsigned int count_near_enemies(unsigned int k, unsigned int m);
unsigned char update_pixel_status(unsigned int i, unsigned int j, unsigned int num_enemies_dead,unsigned int max_active_pixels_reborn, unsigned int *seed);
void copy_sides(void);
unsigned int process_next_step(unsigned int num_enemies_dead, unsigned int max_active_pixels_reborn);
void fill_image_scaled(uint8_t *image);
unsigned char spawn_active_pixel(unsigned int k, unsigned int m, unsigned int *seed);
unsigned int count_active_neighbors_squad(unsigned int k, unsigned int m, unsigned char squad);

unsigned char spawn_pixel(float p1, float p2, unsigned int *seed)
{
    float coin1 = ((float) rand_r(seed)) / RAND_MAX;

    /* with probability p1, keep cell empty */
    if (coin1 < p1) {
        return 0;
    }

    float coin2 = ((float) rand_r(seed)) / RAND_MAX;
    unsigned char pixel_status = (coin2 > p2) ? 'A' : 'B';

    //printf("spawning pixel with status: %c\n", pixel_status);
    //fflush(stdout);

    return pixel_status;
}

unsigned char spawn_active_pixel(unsigned int k, unsigned int m, unsigned int *seed)
{
    unsigned int a = count_active_neighbors_squad(k, m, 'A');
    unsigned int b = count_active_neighbors_squad(k, m, 'B');

    unsigned int total = a + b;

    if (total == 0) {
        // nessun vicino
        return (rand_r(seed) / (float)RAND_MAX > 0.5) ? 'A' : 'B';
    }

    float probA = (float)a / total;

    float coin = ((float) rand_r(seed)) / RAND_MAX;

    return (coin < probA) ? 'A' : 'B';
}

void init(float p1, float p2)
{
    unsigned int seed = (unsigned int) time(NULL);

    printf("initializing grid, p1: %f - p2: %f\n", p1, p2);
    fflush(stdout);

    for (int i = 0; i < SIZE + 2; i++) {
        for (int j = 0; j < SIZE + 2; j++) {
            grid[cur_step][i][j] = 0;
        }
    }

    for (int i = TOP; i <= BOTTOM; i++) {
        for (int j = LEFT; j <= RIGHT; j++) {
            grid[cur_step][i][j] = spawn_pixel(p1, p2, &seed);
        }
    }
}

unsigned int count_active_neighbors(unsigned int k, unsigned int m)
{
  return count_active_neighbors_squad(k,m,'A') + count_active_neighbors_squad(k,m,'B');
}

unsigned int count_active_neighbors_squad(unsigned int k, unsigned int m, unsigned char squad)
{
    if(squad != 'A' && squad != 'B') {
      printf("error in count active neighbors squad, parameter squad must be either A or B");
      abort();
    }

    unsigned int count_squad_pixels = 0;
    for (int i = k - 1; i <= k + 1; i++) {
        for (int j = m - 1; j <= m + 1; j++) {
            if (i == k && j == m) {
                continue;
            }

            if (grid[cur_step][i][j] == squad) {
	      count_squad_pixels++;
            }
        }
    }

    return count_squad_pixels;
}

unsigned int count_near_enemies(unsigned int k, unsigned int m)
{

    if (k > SIZE + 1 || m > SIZE + 1) {
      printf("k,m OUT OF BOUNDS: [%u,%u]\n", k, m);
      abort();
    }
  
    unsigned char pixel_status = grid[cur_step][k][m];

    //printf("Counting enemies of pixel [%d,%d] - pixel status: %c\n", k,m,pixel_status);

    if (pixel_status != 'A' && pixel_status != 'B') {
      //printf("pixel [%d,%d] is not of squad A or B - pixel status: %c\n", k,m,pixel_status);
        return 0;
    }

    unsigned int num_enemies = 0;

    for (int i = k - 1; i <= k + 1; i++) {
        for (int j = m - 1; j <= m + 1; j++) {

	    if (i > SIZE + 1 || j > SIZE + 1) {
              printf("i,j OUT OF BOUNDS: [%u,%u]\n", i, j);
              abort();
            }
	  
            if (i == k && j == m) {
                continue;
            }

            unsigned char neighbor = grid[cur_step][i][j];

	    //char display = (neighbor == 0) ? '.' : neighbor;

	    //printf("pixel [%d,%d] - checking neighbor [%d,%d] - pixel status: %c\n",k,m,i,j,display);

            if ((neighbor == 'A' || neighbor == 'B') && pixel_status != neighbor) {
                num_enemies++;
            }
        }
    }

    //printf("pixel [%d,%d] - number_of_enemies: %d\n", k,m, num_enemies);

    return num_enemies;
}

unsigned char update_pixel_status(unsigned int i, unsigned int j, unsigned int num_enemies_dead, unsigned int max_active_pixels_reborn, unsigned int *seed)
{
    unsigned char cur_pix_status = grid[cur_step][i][j];

    if ((cur_pix_status == 0 || cur_pix_status == 'D') &&
	count_active_neighbors(i, j) <= max_active_pixels_reborn) {

      float coin = ((float) rand_r(seed)) / RAND_MAX;
      if (coin < 0.5f)
        return spawn_active_pixel(i,j,seed);
      else
        return cur_pix_status;
    }

    if (cur_pix_status == 'A' || cur_pix_status == 'B') {
        unsigned int num_enemies = count_near_enemies(i, j);
        unsigned char new_pixel_status = (num_enemies >= num_enemies_dead) ? 'D' : cur_pix_status;
	/*
	if(new_pixel_status == 'D') {
	  printf("pixel [%u,%u] of squad %c - num_of_enemies: %u - pixel is now dead\n",i, j, cur_pix_status, num_enemies);
	}
	*/
        return new_pixel_status;
    }

    //printf("pixel [%u,%u] unchanged, status: %c\n", i, j, cur_pix_status);
    return cur_pix_status;
}

void copy_sides(void)
{
    const int HALO_TOP = TOP - 1;
    const int HALO_BOTTOM = BOTTOM + 1;
    const int HALO_LEFT = LEFT - 1;
    const int HALO_RIGHT = RIGHT + 1;

    for (int j = LEFT; j <= RIGHT; j++) {
        grid[cur_step][HALO_TOP][j] = grid[cur_step][BOTTOM][j];
        grid[cur_step][HALO_BOTTOM][j] = grid[cur_step][TOP][j];
    }

    for (int i = TOP; i <= BOTTOM; i++) {
        grid[cur_step][i][HALO_LEFT] = grid[cur_step][i][RIGHT];
        grid[cur_step][i][HALO_RIGHT] = grid[cur_step][i][LEFT];
    }

    grid[cur_step][HALO_TOP][HALO_LEFT] = grid[cur_step][BOTTOM][RIGHT];
    grid[cur_step][HALO_TOP][HALO_RIGHT] = grid[cur_step][BOTTOM][LEFT];
    grid[cur_step][HALO_BOTTOM][HALO_LEFT] = grid[cur_step][TOP][RIGHT];
    grid[cur_step][HALO_BOTTOM][HALO_RIGHT] = grid[cur_step][TOP][LEFT];
}

unsigned int process_next_step(unsigned int num_enemies_dead, unsigned int max_active_pixels_reborn)
{
    unsigned int next = 1 - cur_step;
    unsigned int changes = 0;

    #pragma omp parallel
    {
        unsigned int seed = (unsigned int)time(NULL)
                  ^ (unsigned int)(omp_get_thread_num() + 1)
                  ^ (unsigned int)(cur_step * 7919);
	
        unsigned int local_changes = 0;

        #pragma omp for collapse(2)
        for (int i = TOP; i <= BOTTOM; i++) {
            for (int j = LEFT; j <= RIGHT; j++) {
                unsigned char new_status =
		  update_pixel_status(i, j, num_enemies_dead, max_active_pixels_reborn, &seed);

                grid[next][i][j] = new_status;

                if (new_status != grid[cur_step][i][j]) {
                    local_changes++;
                }
            }
        }

        #pragma omp atomic
        changes += local_changes;
    }

    cur_step = next;
    return changes;
}

void fill_image_scaled(uint8_t *image)
{
    int out_width = SIZE * SCALE;

    for (int i = TOP; i <= BOTTOM; i++) {
        for (int j = LEFT; j <= RIGHT; j++) {
            unsigned char c = grid[cur_step][i][j];

            uint8_t r, g, b, a;
            if (c == 'A') {
                r = 255; g = 0;   b = 0;   a = 255;
            } else if (c == 'B') {
                r = 0;   g = 0;   b = 255; a = 255;
            } else if (c == 'D') {
                r = 120; g = 120; b = 120; a = 255;
            } else {
                r = 255; g = 255; b = 255; a = 255;
            }

            int cell_y = (i - 1) * SCALE;
            int cell_x = (j - 1) * SCALE;

            for (int dy = 0; dy < SCALE; dy++) {
                for (int dx = 0; dx < SCALE; dx++) {
                    int px = cell_x + dx;
                    int py = cell_y + dy;
                    int idx = (py * out_width + px) * 4;

                    image[idx + 0] = r;
                    image[idx + 1] = g;
                    image[idx + 2] = b;
                    image[idx + 3] = a;
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    int s, nsteps;
    float p1 = 0.5f, p2 = 0.5f;
    int num_neighbor_dead = 4;
    int max_active_pixels_reborn = 3;

    if (argc < 2 || argc > 6) {
        fprintf(stderr, "Usage: %s nsteps [p1] [p2] [num_neighbor_dead]\n", argv[0]);
        return EXIT_FAILURE;
    }

    nsteps = atoi(argv[1]);

    if (argc >= 3) p1 = (float)atof(argv[2]);
    if (argc >= 4) p2 = (float)atof(argv[3]);
    if (argc >= 5) num_neighbor_dead = atoi(argv[4]);
    if (argc == 6) max_active_pixels_reborn = atoi(argv[5]);

    printf("Starting robowar stencil - parameters: [num_steps: %d] - [num_neighbor_dead: %d]\n",
           nsteps, num_neighbor_dead);
    fflush(stdout);

    init(p1, p2);

    int out_width = SIZE * SCALE;
    int out_height = SIZE * SCALE;
    uint8_t *image = malloc((size_t)out_width * out_height * 4);
    if (!image) {
        fprintf(stderr, "Errore allocazione immagine\n");
        return EXIT_FAILURE;
    }

    GifWriter writer;
    if (!GifBegin(&writer, "robowar.gif", out_width, out_height, FRAME_DELAY, 8, false)) {
        fprintf(stderr, "Errore creazione GIF\n");
        free(image);
        return EXIT_FAILURE;
    }

    double start_time = omp_get_wtime();

    fill_image_scaled(image);
    GifWriteFrame(&writer, image, out_width, out_height, FRAME_DELAY, 8, false);

    for (s = 0; s < nsteps; s++) {
      copy_sides();
      unsigned int changes = process_next_step(num_neighbor_dead, max_active_pixels_reborn);

      printf("Step %d - changes: %u\n", s + 1, changes);

      fill_image_scaled(image);
      GifWriteFrame(&writer, image, out_width, out_height, FRAME_DELAY, 8, false);

      if (changes == 0) {
          printf("Simulazione stabilizzata allo step %d\n", s + 1);
          break;
      }
    }

    double end_time = omp_get_wtime();

    printf("Execution time: %f\n", end_time - start_time);
    fflush(stdout);

    GifEnd(&writer);
    free(image);

    return EXIT_SUCCESS;
}
