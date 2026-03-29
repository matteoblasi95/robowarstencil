#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

#define SIZE 5
#define NSTATES 2

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

unsigned char spawn_pixel(float p1, float p2);
void init(float p1, float p2);
int pixels_nearby(int k, int m);
int count_near_enemies(int k, int m);
unsigned char update_pixel_status(int i, int j, int num_enemies_dead, float spawn_p1, float spawn_p2);
void copy_sides(void);
void process_next_step(int num_enemies_dead, float spawn_p1, float spawn_p2);
void write_pbm(const char *fname);

unsigned char spawn_pixel(float p1, float p2)
{
    float coin1 = ((float) rand()) / RAND_MAX;

    /* with probability p1, keep cell empty */
    if (coin1 < p1) {
        return 0;
    }

    float coin2 = ((float) rand()) / RAND_MAX;
    char pixel_status = (coin2 > p2) ? 'A' : 'B';
    printf("spawning pixel with status: %c\n", pixel_status);
    return pixel_status;
}

void init(float p1, float p2)
{
  printf("initializing grid, p1: %f - p2: %f", p1, p2);
    for (int i = 0; i < SIZE + 2; i++) {
        for (int j = 0; j < SIZE + 2; j++) {
            grid[cur_step][i][j] = 0;
        }
    }

    for (int i = TOP; i <= BOTTOM; i++) {
        for (int j = LEFT; j <= RIGHT; j++) {
            grid[cur_step][i][j] = spawn_pixel(p1, p2);
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

            if ((pixel_status == 'A' && neighbor == 'B') ||
                (pixel_status == 'B' && neighbor == 'A')) {
                num_enemies++;
            }
        }
    }

    return num_enemies;
}

unsigned char update_pixel_status(int i, int j, int num_enemies_dead, float spawn_p1, float spawn_p2)
{
    unsigned char cur_pix_status = grid[cur_step][i][j];

    if (cur_pix_status == 'D') {
      printf("pixel [%d,%d] is already dead - remains dead\n", i , j);
        return 'D';
    }

    if (cur_pix_status == 0 && !pixels_nearby(i, j)) {
        char spawned_pixel = spawn_pixel(spawn_p1, spawn_p2);
	printf("pixel [%d,%d] is empty - spawned pixel: %c\n", i,j, spawned_pixel);
	return  spawned_pixel;
    }

    if (cur_pix_status == 'A' || cur_pix_status == 'B') {
        int num_enemies = count_near_enemies(i, j);
        char new_pixel_status = (num_enemies >= num_enemies_dead) ? 'D' : cur_pix_status;
	printf("pixel [%d,%d]  of squad %c - new status: %c\n", i,j, cur_pix_status, new_pixel_status);
	return new_pixel_status;
    }

    printf("pixel [%d,%d]  of squad %c - pixel remains of same status: %c\n", i,j, cur_pix_status, cur_pix_status);

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
    printf("Processing step %d\n", next);

    for (int i = TOP; i <= BOTTOM; i++) {
        for (int j = LEFT; j <= RIGHT; j++) {
            grid[next][i][j] = update_pixel_status(i, j, num_enemies_dead, spawn_p1, spawn_p2);
        }
    }

    cur_step = next;
}

void write_pbm(const char *fname)
{
    FILE *f = fopen(fname, "w");
    if (!f) {
        printf("Cannot open %s for writing\n", fname);
        abort();
    }

    fprintf(f, "P1\n");
    fprintf(f, "# produced by simulation\n");
    fprintf(f, "%d %d\n", SIZE, SIZE);

    for (int i = TOP; i <= BOTTOM; i++) {
        for (int j = LEFT; j <= RIGHT; j++) {
            unsigned char c = grid[cur_step][i][j];
            int bit = (c == 'A' || c == 'B') ? 1 : 0;
            fprintf(f, "%d ", bit);
        }
        fprintf(f, "\n");
    }

    fclose(f);
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

    if (argc >= 3) {
        p1 = (float) atof(argv[2]);
    }
    if (argc >= 4) {
        p2 = (float) atof(argv[3]);
    }
    if (argc == 5) {
        num_neighbor_dead = atoi(argv[4]);
    }

    srand((unsigned) time(NULL));

    char fname[BUFSIZE];

    printf("Starting robowar stencil - parameters: [num_steps: %d] - [num_neighbor_dead: %d]\n", nsteps, num_neighbor_dead);

    init(p1, p2);

    for (s = 0; s < nsteps; s++) {
        snprintf(fname, BUFSIZE, "gol%04d.pbm", s);
        write_pbm(fname);
        copy_sides();
        process_next_step(num_neighbor_dead, p1, p2);
    }

    return EXIT_SUCCESS;
}
