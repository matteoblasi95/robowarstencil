#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <omp.h>
#include "gif.h"

#define GIF_IMPLEMENTATION
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

const int LEFT = 1;
const int RIGHT = SIZE;
const int TOP = 1;
const int BOTTOM = SIZE;

/* double buffer with 1-cell halo on each side */
unsigned char grid[NSTATES][SIZE + 2][SIZE + 2];
int cur_step = 0;

unsigned char spawn_pixel(float p1, float p2, unsigned int *seed);
void init(float p1, float p2);
int pixels_nearby(int k, int m);
int count_near_enemies(int k, int m);
unsigned char update_pixel_status(int i, int j, int num_enemies_dead,
                                  float spawn_p1, float spawn_p2,
                                  unsigned int *seed);
void copy_sides(void);
void process_next_step(int num_enemies_dead, float spawn_p1, float spawn_p2);
void write_ppm_scaled(const char *fname);

unsigned char spawn_pixel(float p1, float p2, unsigned int *seed)
{
    float coin1 = ((float) rand_r(seed)) / RAND_MAX;

    /* with probability p1, keep cell empty */
    if (coin1 < p1) {
        return 0;
    }

    float coin2 = ((float) rand_r(seed)) / RAND_MAX;
    return (coin2 > p2) ? 'A' : 'B';
}

void init(float p1, float p2)
{
    unsigned int seed = (unsigned int) time(NULL);
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

int pixels_nearby(int k, int m)
{
    for (int i = k - 1; i <= k + 1; i++) {
        for (int j = m - 1; j <= m + 1; j++) {
            if (i == k && j == m) {
                continue;
            }

            if (grid[cur_step][i][j] == 'A' || grid[cur_step][i][j] == 'B') {
                return 1;
            }
        }
    }

    return 0;
}

int count_near_enemies(int k, int m)
{
    unsigned char pixel_status = grid[cur_step][k][m];

    if (pixel_status != 'A' && pixel_status != 'B') {
        return 0;
    }

    int num_enemies = 0;

    for (int i = k - 1; i <= k + 1; i++) {
        for (int j = m - 1; j <= m + 1; j++) {
            if (i == k && j == m) {
                continue;
            }

            unsigned char neighbor = grid[cur_step][i][j];

	    if((neighbor == 'A' || neighbor == 'B') && pixel_status != neighbor) {
	      num_enemies++;
	    }
        }
    }

    return num_enemies;
}

unsigned char update_pixel_status(int i, int j, int num_enemies_dead, float spawn_p1, float spawn_p2, unsigned int *seed)
{
    unsigned char cur_pix_status = grid[cur_step][i][j];

    if (cur_pix_status == 'D') {
        return 'D';
    }

    if (cur_pix_status == 0 && !pixels_nearby(i, j)) {
      return spawn_pixel(spawn_p1, spawn_p2, seed);
    }

    if (cur_pix_status == 'A' || cur_pix_status == 'B') {
        int num_enemies = count_near_enemies(i, j);
        return (num_enemies >= num_enemies_dead) ? 'D' : cur_pix_status;
    }

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

void process_next_step(int num_enemies_dead, float spawn_p1, float spawn_p2)
{
    int next = 1 - cur_step;
    #pragma omp parallel
    {
      unsigned int seed = 1000 + omp_get_thread_num();
      #pragma omp for collapse(2)
      for (int i = TOP; i <= BOTTOM; i++) {
          for (int j = LEFT; j <= RIGHT; j++) {
            grid[next][i][j] = update_pixel_status(i, j, num_enemies_dead, spawn_p1, spawn_p2, &seed);
          }
      }
    }

    cur_step = next;
}

#define CELL_PIXELS 20

void write_ppm_scaled(const char *fname)
{
    FILE *f = fopen(fname, "w");
    if (!f) {
        printf("Cannot open %s for writing\n", fname);
        abort();
    }

    int out_w = SIZE * CELL_PIXELS;
    int out_h = SIZE * CELL_PIXELS;

    fprintf(f, "P3\n");
    fprintf(f, "%d %d\n", out_w, out_h);
    fprintf(f, "255\n");

    for (int i = TOP; i <= BOTTOM; i++) {
        for (int sy = 0; sy < CELL_PIXELS; sy++) {
            for (int j = LEFT; j <= RIGHT; j++) {
                unsigned char c = grid[cur_step][i][j];
                int r, g, b;

                if (c == 'A') {
                    r = 255; g = 0; b = 0;       /* rosso */
                } else if (c == 'B') {
                    r = 0; g = 0; b = 255;       /* blu */
                } else if (c == 'D') {
                    r = 120; g = 120; b = 120;   /* grigio */
                } else {
                    r = 255; g = 255; b = 255;   /* bianco */
                }

                for (int sx = 0; sx < CELL_PIXELS; sx++) {
                    fprintf(f, "%d %d %d ", r, g, b);
                }
            }
            fprintf(f, "\n");
        }
    }

    fclose(f);
}


uint8_t image[SIZE * SIZE * 4]; // RGBA

void fill_image(uint8_t *image)
{
    for (int i = TOP; i <= BOTTOM; i++) {
        for (int j = LEFT; j <= RIGHT; j++) {

            int idx = ((i-1)*SIZE + (j-1)) * 4;

            unsigned char c = grid[cur_step][i][j];

            if (c == 'A') {
                image[idx+0] = 255; // R
                image[idx+1] = 0;   // G
                image[idx+2] = 0;   // B
                image[idx+3] = 255;
            } else if (c == 'B') {
                image[idx+0] = 0;
                image[idx+1] = 0;
                image[idx+2] = 255;
                image[idx+3] = 255;
            } else if (c == 'D') {
                image[idx+0] = 120;
                image[idx+1] = 120;
                image[idx+2] = 120;
                image[idx+3] = 255;
            } else {
                image[idx+0] = 255;
                image[idx+1] = 255;
                image[idx+2] = 255;
                image[idx+3] = 255;
            }
        }
    }
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


#define BUFSIZE 128

int main(int argc, char *argv[])
{
    int s, nsteps;
    float p1 = 0.5f, p2 = 0.5f;
    int num_neighbor_dead = 3;

    if (argc < 2 || argc > 5) {
        fprintf(stderr, "Usage: %s nsteps [p1] [p2] [num_neighbor_dead]\n", argv[0]);
        return EXIT_FAILURE;
    }

    nsteps = atoi(argv[1]);

    if (argc >= 3) p1 = atof(argv[2]);
    if (argc >= 4) p2 = atof(argv[3]);
    if (argc == 5) num_neighbor_dead = atoi(argv[4]);

    init(p1, p2);

    /* buffer immagine RGBA */
    int out_width = SIZE * SCALE;
    int out_height = SIZE * SCALE;
    uint8_t *image = malloc((size_t)out_width * out_height * 4);
    if (!image) {
      fprintf(stderr, "Errore allocazione immagine\n");
      return EXIT_FAILURE;
   }

    /* crea GIF */
    GifWriter writer;
    GifBegin(&writer, "robowar.gif", out_width, out_height, FRAME_DELAY, 8, false);

    double start_time = omp_get_wtime();

    for (s = 0; s < nsteps; s++) {

        /* converti griglia → immagine */
        fill_image_scaled(image);
        GifWriteFrame(&writer, image, out_width, out_height, FRAME_DELAY, 8, false);

        copy_sides();
        process_next_step(num_neighbor_dead, p1, p2);
    }

    double end_time = omp_get_wtime();

    printf("Execution time: %f", end_time-start_time);

    GifEnd(&writer);
    free(image);

    return EXIT_SUCCESS;
}
