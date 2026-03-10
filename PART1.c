#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>

double next_exp(float lambda, float upper){

    while (val > upper){
        double r = drand(48); 
        double val = -log(r)/lambda;
        return val;
    }
}

int main( int argc, char ** argv ){

    if (argc != 6){
        fprintf(stderr, "ERROR: invalid number of arguments\n");
        return EXIT_FAILURE;
    }

    int n = atoi(argv[1]);
    int ncpu = atoi(argv[2]);
    int seed = atoi(argv[3]);
    double lambda = atof(argv[4]);
    double upper = atof(argv[5]);

    if (n <= 0) {
        fprintf(stderr, "ERROR: number of processes must be > 0\n");
        return EXIT_FAILURE;
    }

    if (ncpu < 0 || ncpu > n) {
        fprintf(stderr, "ERROR: invalid number of CPU-bound processes\n");
        return EXIT_FAILURE;
    }

    if (lambda <= 0.0) {
        fprintf(stderr, "ERROR: lambda must be > 0\n");
        return EXIT_FAILURE;
    }

    if (upper <= 0.0) {
        fprintf(stderr, "ERROR: upper bound must be > 0\n");
        return EXIT_FAILURE;
    }

    srand48(seed);

    //HEADER
    if (ncpu == 1){
        printf("<<< -- process set (n=%d) with %d CPU-bound process\n", n, ncpu );
    } else{
        printf("<<< -- process set (n=%d) with %d CPU-bound processes\n", n, ncpu );
    }
    printf("<<< -- seed=%d; lambda=%06f; upper bound=%.0f\n", seed, lambda, upper);


    return EXIT_SUCCESS;
}