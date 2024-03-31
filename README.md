# README

Tema 1 APD
Bogdan Elena-Catalina

    The file tema1_par.c contains the parallelization of the algorithm that generates contours of topological maps using the Marching Squares Algorithm.

The basic functions of this approach are those contained in the initial skeleton, which are subsequently modified to allow the parallel execution of the algorithm, with each thread handling certain lines of pixels of the input image, determined by the start and end values.

To avoid using global variables, a structure called "iteration" was created, which contains all the variables necessary for a thread. In the main function, a vector of structures is used, allowing access to resources without triggering data races.

The functions rescale, sample_grid, and march allow parallelization, so an auxiliary function alg is created to be passed to each thread created.

1. Rescale: Since only images with large dimensions are modified, this case is handled in the main function by assigning the "scaled_image" field the initial image if it has suitable dimensions. Otherwise, following the resizing algorithm, it will be updated with the newly calculated values.

2. Sample_grid: Using the scaled image, we want to build its grid square. Similarly, the initial algorithm is respected, and memory is allocated in the main function.

3. March: Updates the contour of the scaled image.

Finally, after waiting for the threads, the result is written to the output file, and the allocated memory is freed.
