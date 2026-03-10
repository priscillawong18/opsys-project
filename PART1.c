#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>

double next_exp(double lambda, double upper) {
    double r, val;

    do {
        r = drand48();
        val = -log(r) / lambda;
    } while (val > upper);

    return val;
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

    // char wletter = 'A' + (11/10);
    // printf("letter: %c\n", wletter);
    for (int i = 0; i < n; i++) {
        char letter = 'A' + (i / 10);
        int digit = i % 10;
        int is_cpu_bound = (i >= (n - ncpu));

        int arrival = (int) floor(next_exp(lambda, upper));
        int bursts = (int) ceil(16 * drand48());

        if (is_cpu_bound) {
            printf("CPU-bound process %c%d: arrival time %dms; %d CPU bursts:\n",
                letter, digit, arrival, bursts);
        } else {
            printf("I/O-bound process %c%d: arrival time %dms; %d CPU bursts:\n",
                letter, digit, arrival, bursts);
        }
    }


    return EXIT_SUCCESS;
}