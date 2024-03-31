#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <math.h>

#define CONTOUR_CONFIG_COUNT    16
#define FILENAME_MAX_SIZE       50
#define STEP                    8
#define SIGMA                   200
#define RESCALE_X               2048
#define RESCALE_Y               2048


#define CLAMP(v, min, max) if(v < min) { v = min; } else if(v > max) { v = max; }

typedef struct iteration{
    ppm_image *image;
    ppm_image **contour_map;
    ppm_image *scaled_image;
    unsigned char **grid;
    const char *output_filename;
    long nr_threads;
    long id;
    int step_x;
    int step_y;
    pthread_barrier_t* barrier;
};

void free_resources(ppm_image *image, ppm_image **contour_map, unsigned char **grid, int step_x) {
    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++) {
        free(contour_map[i]->data);
        free(contour_map[i]);
    }
    free(contour_map);

    for (int i = 0; i <= image->x / step_x; i++) {
        free(grid[i]);
    }
    free(grid);

    free(image->data);
    free(image);
}

ppm_image **init_contour_map() {
    ppm_image **map = (ppm_image **)malloc(CONTOUR_CONFIG_COUNT * sizeof(ppm_image *));
    if (!map) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++) {
        char filename[FILENAME_MAX_SIZE];
        sprintf(filename, "./contours/%d.ppm", i);
        map[i] = read_ppm(filename);
    }

    return map;
}
void rescale_image(struct iteration* it) {
    uint8_t sample[3];

    // we only rescale downwards
    // it is checked in main function whether the image fits the parameters

    if (it->image->x > RESCALE_X && it->image->y > RESCALE_Y) {
    
        int start = ceil((double)it->scaled_image->x/(double)it->nr_threads) * it->id;
        int end =  fmin(ceil((double)it->scaled_image->x/(double)it->nr_threads)*(it->id + 1), it->scaled_image->x);
    
        // use bicubic interpolation for scaling
        for (int i = start; i < end; i++) {
            for (int j = 0; j < it->scaled_image->y; j++) {
                float u = (float)i / (float)(it->scaled_image->x - 1);
                float v = (float)j / (float)(it->scaled_image->y - 1);
                sample_bicubic(it->image, u, v, sample);

                it->scaled_image->data[i * it->scaled_image->y + j].red = sample[0];
                it->scaled_image->data[i * it->scaled_image->y + j].green = sample[1];
                it->scaled_image->data[i * it->scaled_image->y + j].blue = sample[2];
            }
        }
    }
}
void update_image(ppm_image *image, ppm_image *contour, int x, int y) {
    for (int i = 0; i < contour->x; i++) {
        for (int j = 0; j < contour->y; j++) {
            int contour_pixel_index = contour->x * i + j;
            int image_pixel_index = (x + i) * image->y + y + j;

            image->data[image_pixel_index].red = contour->data[contour_pixel_index].red;
            image->data[image_pixel_index].green = contour->data[contour_pixel_index].green;
            image->data[image_pixel_index].blue = contour->data[contour_pixel_index].blue;
        }
    }
}

void march(struct iteration* it) {

    int p = it->scaled_image->x / it->step_x;
    int q = it->scaled_image->y / it->step_y;
    int start = ceil((double)p/(double)it->nr_threads) * it->id;
	int end =  fmin(ceil((double)p/(double)it->nr_threads)*(it->id + 1), p);

    for (int i = start; i < end; i++) {
        for (int j = 0; j < q; j++) {
            unsigned char k = 8 * it->grid[i][j] + 4 * it->grid[i][j + 1] + 2 * it->grid[i + 1][j + 1] + 1 * it->grid[i + 1][j];
            update_image(it->scaled_image, it->contour_map[k], i * it->step_x, j * it->step_y);
        }
    }
    
} 
void sample_grid(struct iteration* it, unsigned char sigma) {
    int p = it->scaled_image->x / it->step_x;
    int q = it->scaled_image->y / it->step_y;

    int start = ceil((double)p/(double)it->nr_threads) * it->id;
	int end =  fmin(ceil((double)p/(double)it->nr_threads)*(it->id + 1), p);

    for (int i = start; i < end; i++) { 
        for (int j = 0; j < q; j++) {
            ppm_pixel curr_pixel = it->scaled_image->data[i * it->step_x * it->scaled_image->y + j * it->step_y];

            unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

            if (curr_color > sigma) {
                it->grid[i][j] = 0;
            } else {
                it->grid[i][j] = 1;
            }
        }
    }
    it->grid[p][q] = 0;

    // last sample points have no neighbors below / to the right, so we use pixels on the
    // last row / column of the input image for them
    for (int i = start; i < end; i++) { // last column
        ppm_pixel curr_pixel = it->scaled_image->data[i * it->step_x * it->scaled_image->y + it->scaled_image->x - 1]; 

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > sigma) {
            it->grid[i][q] = 0;
        } else {
            it->grid[i][q] = 1;
        }
    }
    for (int j = 0; j < q; j++) { // last row of pixels
        ppm_pixel curr_pixel = it->scaled_image->data[(it->scaled_image->x - 1) * it->scaled_image->y + j * it->step_y];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > sigma) {
            it->grid[p][j] = 0;
        } else {
            it->grid[p][j] = 1;
        }
    }
}

void* alg(void *arg) {
    struct iteration *it = (struct iteration*)(arg);
    if(it != NULL) {
        // 1. rescale
        rescale_image(it);
        int s = pthread_barrier_wait(it->barrier);
        // 2. grid sample
        sample_grid(it, SIGMA);
        s = pthread_barrier_wait(it->barrier);
        // 3. march
        march(it);
        
    } else {
        fprintf(stderr, "Invalid pointer passed to alg.\n");
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: ./tema1 <in_file> <out_file> <P>\n");
        return 1;
    }
    int r;
    void* status;
    long P = (long)atoi(argv[3]); // number of threads

    struct iteration* it_vector = (struct iteration*)malloc(sizeof(struct iteration) * P);
    pthread_t threads[P];

    ppm_image** contour_map;
    contour_map = init_contour_map();

    // allocate memory for image to be rescaled
    ppm_image*image = read_ppm(argv[1]);

    //scaled_image = (ppm_image *)malloc(sizeof(ppm_image)) ;
    ppm_image*scaled_image = read_ppm(argv[1]);
    
    // the image that needs to be rescaled
    ppm_image *new_image = (ppm_image *)malloc(sizeof(ppm_image));
    if (!new_image) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }
    new_image->x = RESCALE_X;
    new_image->y = RESCALE_Y;

    new_image->data = (ppm_pixel*)malloc(new_image->x * new_image->y * sizeof(ppm_pixel));
    if (!new_image) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }
    // allocate memory for grid
    int p,q;
    if (image->x <= RESCALE_X && image->y <= RESCALE_Y) {
        p = image->x / STEP;
        q = image->y / STEP;  
    } else {
        p = new_image->x / STEP;
        q = new_image->y / STEP;
    }
    unsigned char **grid = (unsigned char **)malloc((p + 1) * sizeof(unsigned char*));
    if (!grid) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    for (int i = 0; i <= p ; i++) {
        grid[i] = (unsigned char *)malloc((q + 1) * sizeof(unsigned char));
        if (!grid[i]) {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
    }
    
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, P);

    for(long id = 0; id < P; id ++) {
        it_vector[id].id = id;
        it_vector[id].nr_threads = P;
        it_vector[id].step_x = STEP;
        it_vector[id].step_y = STEP;
        it_vector[id].output_filename = argv[2];
        it_vector[id].image = image;
        if (image->x <= RESCALE_X && image->y <= RESCALE_Y) {
            it_vector[id].scaled_image = scaled_image;
        } else {
            it_vector[id].scaled_image = new_image;
        }
        it_vector[id].grid = grid;
        it_vector[id].contour_map = contour_map;
        it_vector[id].barrier = &barrier; 

        

        r = pthread_create(&threads[id], NULL, alg, (void *)&it_vector[id]);

        if(r) {
            printf("Eroare la crearea thread-ului %ld\n", id);
            exit(-1);
        }
    }
    for (long id = 0; id < P; id ++) {
        r = pthread_join(threads[id], &status);

        if(r) {
            printf("Eroare la asteptarea thread-ului %ld\n", id);
            exit(-1);
        }
    }
    write_ppm(it_vector[P-1].scaled_image, it_vector[P-1].output_filename);
    
    pthread_barrier_destroy(&barrier);
    
    free_resources(it_vector[P-1].scaled_image, it_vector[P-1].contour_map, it_vector[P-1].grid, it_vector[P-1].step_x);
    free(it_vector);

    return 0;
}