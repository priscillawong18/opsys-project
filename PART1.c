#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>

typedef struct {
    char id[3];
    int arrivalTime;
    int numCpuBursts;
    int isCpuBound;
    int *cpuBursts;
    int *ioBursts;
} Process;


double next_exp(double lambda, double upper) {
    double r, val;

    do {
        r = drand48();
        val = -log(r) / lambda;
    } while (val > upper);

    return val;
}

Process* generateProcess(int n, int ncpu, double lambda, double upper ){

    //array of processes
    Process* processArray = (Process*) calloc(n, sizeof(Process));
    if (processArray == NULL) {
        fprintf(stderr, "ERROR: memory allocation failed\n");
        return NULL;
    }

    for (int i = 0; i<n; i++){
        Process* p = &processArray[i];

        char letter = 'A' + (i / 10);
        int digit = i % 10;
        p->isCpuBound = (i >= (n - ncpu));

        p->arrivalTime = floor(next_exp(lambda, upper)); 
        p->numCpuBursts = ceil(16 * drand48()); 

        sprintf(p->id, "%c%d", letter, digit);
        p-> cpuBursts = calloc(p->numCpuBursts, sizeof(int));
        if (p->cpuBursts == NULL) {
            fprintf(stderr, "ERROR: memory allocation failed\n");
           // free(process);
            return NULL;
        }
        p-> ioBursts = calloc((p->numCpuBursts - 1), sizeof(int));
        if (p->ioBursts == NULL) {
            fprintf(stderr, "ERROR: memory allocation failed\n");
           // free(process);
            return NULL;
        }

        for (int j = 0; j < p->numCpuBursts; j++) {
            int cpuBurstTime = (int) ceil(next_exp(lambda, upper));

            if (p->isCpuBound) {
                cpuBurstTime *= 6;
            }

            p->cpuBursts[j] = cpuBurstTime;

            if (j < p->numCpuBursts - 1) {
                int ioBurstTime = (int) ceil(next_exp(lambda, upper));

                if (!p->isCpuBound) {
                    ioBurstTime *= 8;
                }

                p->ioBursts[j] = ioBurstTime;
            }
        }

    }

    return processArray;
}

void output(Process* process, int n, int ncpu, int seed, double lambda, double upper){

    //HEADER
    if (ncpu == 1){
        printf("<<< -- process set (n=%d) with %d CPU-bound process\n", n, ncpu );
    } else{
        printf("<<< -- process set (n=%d) with %d CPU-bound processes\n", n, ncpu );
    }
    printf("<<< -- seed=%d; lambda=%.6f; upper bound=%.0f\n", seed, lambda, upper);

     for(int i = 0; i<n; i++){
        Process* p = &process[i];
        //int is_cpu_bound = (i >= (n - ncpu));

        if(p->isCpuBound){
            if(p->numCpuBursts == 1){
                printf("\nCPU-bound process %s: arrival time %dms; %d CPU burst:\n",
                 p->id , p->arrivalTime, p->numCpuBursts);
            }else{
                printf("\nCPU-bound process %s: arrival time %dms; %d CPU bursts:\n",
                     p->id , p->arrivalTime, p->numCpuBursts); 
            }
        } else{
            if(p->numCpuBursts == 1){
                printf("\nI/O-bound process %s: arrival time %dms; %d CPU burst:\n",
                     p->id, p->arrivalTime, p->numCpuBursts);
            } else{
                printf("\nI/O-bound process %s: arrival time %dms; %d CPU bursts:\n",
                     p->id, p->arrivalTime, p->numCpuBursts);
            }
        }

        for (int j = 0; j< p->numCpuBursts; j++){
            printf("==> CPU burst %dms", p->cpuBursts[j]);
            if(j < p->numCpuBursts - 1){
                printf(" ==> I/O burst %dms\n", p->ioBursts[j]);
            } else{
                printf("\n");
            }

        }   
        

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

    if (n > 260) {
        fprintf(stderr, "ERROR: number of processes must be <= 260\n");
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

    Process* process = generateProcess(n, ncpu, lambda, upper);
    
    output(process, n, ncpu,  seed,  lambda, upper);

    for(int i = 0; i<n; i++){
        free(process[i].cpuBursts);
        free(process[i].ioBursts);
    }

    free(process);


    return EXIT_SUCCESS;
}