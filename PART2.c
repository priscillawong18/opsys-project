#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

typedef struct {
    char id[3];
    int arrivalTime;
    int numCpuBursts;
    int isCpuBound;
    int *cpuBursts;
    int *ioBursts;
    
    
    char state; 
    int currentBurstIndex; 
    int tau;     
    int predictedRemainingTime;       
    int remainingTime;  
    int timeNextEvent;  
    
    // Statistics Tracking
    int timeEnteredReadyQueue;
    int waitTime;
    int turnaroundTime;
    int contextSwitches;
    int preemptions;
    int rrBurstsCompletedInSlice;
    int rrBurstsTotal;
    int totalCpuTime;
} Process;

typedef struct Node {
    Process* process; 
    struct Node* next;
} Node;

typedef struct {
    Node* head;
    Node* tail;
    int size; 
} Queue;

Queue* createQueue() {
    Queue* queue = (Queue*) malloc(sizeof(Queue));
    if (queue == NULL) {
        fprintf(stderr, "ERROR: memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    return queue;
}

int isEmpty(Queue* q) {
    return q->size == 0;
}

void pushBack(Queue* queue, Process* process) {
    Node* newNode = (Node*) malloc(sizeof(Node));
    if (newNode == NULL) {
        fprintf(stderr, "ERROR: memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    newNode->process = process;
    newNode->next = NULL;
    if (queue->tail == NULL) {
        queue->head = newNode;
        queue->tail = newNode;
    } else {
        queue->tail->next = newNode;
        queue->tail = newNode;
    }
    queue->size++;
}

Process* popFront(Queue* queue) {
    if (queue->head == NULL) return NULL;
    Node* temp = queue->head;
    Process* process = temp->process;
    queue->head = queue->head->next;
    if (queue->head == NULL) queue->tail = NULL;
    free(temp);
    queue->size--;
    return process;
}

// Insert based on shortest remaining time (SRT) or shortest tau (SJF)
void insertSorted(Queue* q, Process* p, int is_srt) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        fprintf(stderr, "ERROR: memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    newNode->process = p;
    newNode->next = NULL;

    int p_priority = is_srt ? p->predictedRemainingTime : p->tau;

    if (isEmpty(q)) {
        q->head = newNode;
        q->tail = newNode;
        q->size++;
        return;
    }

    int head_priority = is_srt ? q->head->process->predictedRemainingTime : q->head->process->tau;

    
    if (p_priority < head_priority || (p_priority == head_priority && strcmp(p->id, q->head->process->id) < 0)) {
        newNode->next = q->head;
        q->head = newNode;
        q->size++;
        return;
    }

    Node* current = q->head;
    while (current->next != NULL) {
        int nextPriority = is_srt ? current->next->process->predictedRemainingTime : current->next->process->tau;
        if (p_priority < nextPriority || (p_priority == nextPriority && strcmp(p->id, current->next->process->id) < 0)) {
            break;
        }
        current = current->next;
    }

    newNode->next = current->next;
    current->next = newNode;
    if (newNode->next == NULL) q->tail = newNode;
    q->size++;
}

void printQueue(Queue* q) {
    if (isEmpty(q)) {
        printf("[Q: -]\n");
        return;
    }
    printf("[Q:");
    Node* current = q->head;
    while (current != NULL) {
        printf(" %s", current->process->id);
        current = current->next;
    }
    printf("]\n");
}

void freeQueue(Queue* q) {
    while (!isEmpty(q)) popFront(q);
    free(q);
}


double next_exp(double lambda, double upper) {
    double r, val;
    do {
        r = drand48();
        val = -log(r) / lambda;
    } while (val > upper);
    return val;
}

Process* generateProcess(int n, int ncpu, double lambda, double upper) {
    Process* processArray = (Process*) calloc(n, sizeof(Process));
    if (processArray == NULL) {
        fprintf(stderr, "ERROR: memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < n; i++) {
        Process* p = &processArray[i];
        char letter = 'A' + (i / 10);
        int digit = i % 10;
        p->isCpuBound = (i >= (n - ncpu));

        p->arrivalTime = floor(next_exp(lambda, upper)); 
        p->numCpuBursts = ceil(16 * drand48()); 

        sprintf(p->id, "%c%d", letter, digit);
        p->cpuBursts = calloc(p->numCpuBursts, sizeof(int));
        p->ioBursts = calloc((p->numCpuBursts - 1), sizeof(int));

        for (int j = 0; j < p->numCpuBursts; j++) {
            int cpuBurstTime = (int) ceil(next_exp(lambda, upper));
            if (p->isCpuBound) cpuBurstTime *= 6;
            p->cpuBursts[j] = cpuBurstTime;

            if (j < p->numCpuBursts - 1) {
                int ioBurstTime = (int) ceil(next_exp(lambda, upper));
                if (!p->isCpuBound) ioBurstTime *= 8;
                p->ioBursts[j] = ioBurstTime;
            }
        }
    }
    return processArray;
}

void outputProcesses(Process* process, int n, int ncpu, int seed, double lambda, double upper, int tcs, float alpha, int timeSlice) {
    printf("<<< -- process set (n=%d) with %d CPU-bound process%s\n", n, ncpu, ncpu == 1 ? "" : "es");
    printf("<<< -- seed=%d; lambda=%.6f; upper bound=%.0f\n", seed, lambda, upper);
    
    if (alpha == -1.0) {
        printf("<<< -- t_cs=%dms; alpha=<n/a>; t_slice=%dms\n\n", tcs, timeSlice);
    } else {
        printf("<<< -- t_cs=%dms; alpha=%.2f; t_slice=%dms\n\n", tcs, alpha, timeSlice);
    }

    for(int i = 0; i < n; i++) {
        Process* p = &process[i];
        const char* type = p->isCpuBound ? "CPU-bound" : "I/O-bound";
        printf("%s process %s: arrival time %dms; %d CPU burst%s\n", 
            type, p->id, p->arrivalTime, p->numCpuBursts, p->numCpuBursts == 1 ? "" : "s");
    }
}


void resetProcesses(Process* procs, int n, float alpha, double lambda) {
    for (int i = 0; i < n; i++) {
        procs[i].state = 'U'; 
        procs[i].currentBurstIndex = 0;
        // In OPT (-1.0), tau perfectly predicts the first actual burst
        procs[i].tau = (alpha == -1.0) ? procs[i].cpuBursts[0] : (int)ceil(1.0 / lambda);
        procs[i].timeNextEvent = procs[i].arrivalTime;
        procs[i].remainingTime = 0;
        procs[i].timeEnteredReadyQueue = 0;
        
        procs[i].waitTime = 0;
        procs[i].turnaroundTime = 0;
        procs[i].contextSwitches = 0;
        procs[i].preemptions = 0;
        procs[i].rrBurstsCompletedInSlice = 0;
        procs[i].rrBurstsTotal = procs[i].numCpuBursts;
        procs[i].totalCpuTime = 0;
    }
}

void runSimulation(Process* procs, int n, const char* algo, int tcs, float alpha, int timeSlice, double lambda, FILE* simout) {
    int t = 0;
    int terminated_count = 0;
    Queue* q = createQueue();
    
    resetProcesses(procs, n, alpha, lambda);

    bool is_fcfs = strcmp(algo, "FCFS") == 0;
    bool is_sjf  = strcmp(algo, "SJF") == 0 || strcmp(algo, "SJF-OPT") == 0;
    bool is_srt  = strcmp(algo, "SRT") == 0 || strcmp(algo, "SRT-OPT") == 0;
    bool is_rr   = strcmp(algo, "RR") == 0;

    Process* cpuProcess = NULL;
    Process* processSwappingIn = NULL;
    Process* processSwappingOut = NULL;

    int timeSwapInCompletes = -1;
    int timeSwapOutCompletes = -1;
    int currentSliceRemaining = 0;

    printf("time %dms: Simulator started for %s ", t, algo);
    printQueue(q);

    while (1) {
        
        // Must wait for the final terminated process to finish swapping out before exiting
        if (terminated_count == n && processSwappingOut == NULL) {
            printf("time %dms: Simulator ended for %s ", t-1, algo);
            printQueue(q);
            break;
        }

        if (cpuProcess != NULL) {
            cpuProcess->remainingTime--;
            cpuProcess->predictedRemainingTime--;
            cpuProcess->totalCpuTime++;
            if (is_rr) currentSliceRemaining--;

            if (cpuProcess->remainingTime == 0) {
                int burstIdx = cpuProcess->currentBurstIndex;
                int actualBurst = cpuProcess->cpuBursts[burstIdx];
                int bursts_left = cpuProcess->numCpuBursts - burstIdx - 1;
                
                cpuProcess->turnaroundTime += (t + (tcs/2)) - cpuProcess->timeEnteredReadyQueue;

                if (bursts_left == 0) {
                    cpuProcess->state = 'T';
                    terminated_count++;
                    // Terminations must print regardless of t >= 10000
                    printf("time %dms: Process %s terminated ", t, cpuProcess->id);
                    printQueue(q);
                } else {
                    cpuProcess->state = 'I';
                    int io_time = cpuProcess->ioBursts[burstIdx];
                    cpuProcess->timeNextEvent = t + (tcs / 2) + io_time;
                    
                    if (t < 10000) {
                        char tauStr[32] = "";
                        if ((is_sjf || is_srt) && alpha != -1.0) sprintf(tauStr, " (tau %dms)", cpuProcess->tau);
                        if (bursts_left == 1) {
                            printf("time %dms: Process %s%s completed a CPU burst; 1 burst to go ", t, cpuProcess->id, tauStr);
                        } else {
                            printf("time %dms: Process %s%s completed a CPU burst; %d bursts to go ", t, cpuProcess->id, tauStr, bursts_left);
                        }
                            printQueue(q);
                    }
                    
                    if ((is_sjf || is_srt) && alpha != -1.0) {
                        int old_tau = cpuProcess->tau;
                        cpuProcess->tau = (int)ceil(alpha * actualBurst + (1.0 - alpha) * old_tau);
                        if (t < 10000) {
                            printf("time %dms: Recalculated tau for process %s to %dms ", t, cpuProcess->id, cpuProcess->tau);
                            printQueue(q);
                        }
                    }

                    if (t < 10000) {
                        printf("time %dms: Process %s switching out of CPU; blocking on I/O until time %dms ", t, cpuProcess->id, cpuProcess->timeNextEvent);
                        printQueue(q);
                    }
                }
                
                if (is_rr && actualBurst <= timeSlice) {
                    cpuProcess->rrBurstsCompletedInSlice++;
                }
                
                processSwappingOut = cpuProcess;
                timeSwapOutCompletes = t + (tcs / 2);
                cpuProcess = NULL;
                
            } else if (is_rr && currentSliceRemaining == 0) {
                if (isEmpty(q)) {
                    if (t < 10000) {
                        printf("time %dms: Time slice expired; no preemption because ready queue is empty ", t);
                        printQueue(q);
                    }
                    currentSliceRemaining = timeSlice; 
                } else {
                    // Preemptions must print regardless of t >= 10000
                    printf("time %dms: Time slice expired; preempting process %s with %dms remaining ", t, cpuProcess->id, cpuProcess->remainingTime);
                    printQueue(q);
                    
                    cpuProcess->state = 'R';
                    cpuProcess->preemptions++;
                    cpuProcess->turnaroundTime += (t + (tcs/2)) - cpuProcess->timeEnteredReadyQueue;
                    
                    processSwappingOut = cpuProcess;
                    timeSwapOutCompletes = t + (tcs / 2);
                    cpuProcess = NULL;
                }
            }
        }

        if (processSwappingOut != NULL && t == timeSwapOutCompletes) {
            if (processSwappingOut->state == 'R') {
                if (is_fcfs || is_rr) pushBack(q, processSwappingOut);
                else insertSorted(q, processSwappingOut, is_srt);
                processSwappingOut->timeEnteredReadyQueue = t;
            }
            processSwappingOut = NULL;
        }

        if (processSwappingIn != NULL && t == timeSwapInCompletes) {
            cpuProcess = processSwappingIn;
            processSwappingIn = NULL;
            cpuProcess->state = 'C'; 
            cpuProcess->contextSwitches++;
            if (is_rr) currentSliceRemaining = timeSlice;

            if (t < 10000) {
                int totalBurst = cpuProcess->cpuBursts[cpuProcess->currentBurstIndex];
                char tauStr[32] = "";
                if ((is_sjf || is_srt) && alpha != -1.0) sprintf(tauStr, " (tau %dms)", cpuProcess->tau);

                if (cpuProcess->remainingTime < totalBurst) {
                    printf("time %dms: Process %s%s started using the CPU for remaining %dms of %dms burst ", 
                        t, cpuProcess->id, tauStr, cpuProcess->remainingTime, totalBurst);
                } else {
                    printf("time %dms: Process %s%s started using the CPU for %dms burst ", 
                        t, cpuProcess->id, tauStr, totalBurst);
                }
                printQueue(q);
            }
        }

        for (int i = 0; i < n; i++) {
            Process* p = &procs[i];
            if (p->state == 'I' && t == p->timeNextEvent) {
                p->state = 'R';
                p->currentBurstIndex++;
                if (p->remainingTime == 0) p->remainingTime = p->cpuBursts[p->currentBurstIndex];
                if (alpha == -1.0) p->tau = p->cpuBursts[p->currentBurstIndex]; // Exact insight for OPT
                p->predictedRemainingTime = p->tau;
                p->timeEnteredReadyQueue = t;

                char tauStr[32] = "";
                if ((is_sjf || is_srt) && alpha != -1.0) sprintf(tauStr, " (tau %dms)", p->tau);

                if (is_srt && cpuProcess != NULL && p->predictedRemainingTime < cpuProcess->predictedRemainingTime) {
                    cpuProcess->state = 'R';
                    cpuProcess->preemptions++;
                    cpuProcess->turnaroundTime += (t + tcs/2) - cpuProcess->timeEnteredReadyQueue;

                    processSwappingOut = cpuProcess;
                    timeSwapOutCompletes = t + (tcs / 2);
                    cpuProcess = NULL;

                    processSwappingIn = p; 
                    timeSwapInCompletes = t + tcs; 
                    p->waitTime += (tcs / 2);

                    const char* rem_str = (alpha == -1.0) ? "remaining time" : "predicted remaining time";
                    
                    printf("time %dms: Process %s%s completed I/O; preempting %s (%s %dms) ", 
                           t, p->id, tauStr, processSwappingOut->id, rem_str, processSwappingOut->predictedRemainingTime);

                    printQueue(q);
                } else {
                    if (is_fcfs || is_rr) pushBack(q, p);
                    else insertSorted(q, p, is_srt);

                    if (t < 10000) {
                        printf("time %dms: Process %s%s completed I/O; added to ready queue ", t, p->id, tauStr);
                        printQueue(q);
                    }
                }
            }
        }

        for (int i = 0; i < n; i++) {
            Process* p = &procs[i];
            if (p->state == 'U' && t == p->timeNextEvent) {
                p->state = 'R';
                if (p->remainingTime == 0) p->remainingTime = p->cpuBursts[p->currentBurstIndex];
                if (alpha == -1.0) p->tau = p->cpuBursts[p->currentBurstIndex]; // Exact insight for OPT
                p->predictedRemainingTime = p->tau;
                p->timeEnteredReadyQueue = t;

                char tauStr[32] = "";
                if ((is_sjf || is_srt) && alpha != -1.0) sprintf(tauStr, " (tau %dms)", p->tau);

                if (is_srt && cpuProcess != NULL && p->predictedRemainingTime < cpuProcess->predictedRemainingTime) {
                    cpuProcess->state = 'R';
                    cpuProcess->preemptions++;
                    cpuProcess->turnaroundTime += (t + tcs/2) - cpuProcess->timeEnteredReadyQueue;

                    processSwappingOut = cpuProcess;
                    timeSwapOutCompletes = t + (tcs / 2);
                    cpuProcess = NULL;

                    processSwappingIn = p;
                    timeSwapInCompletes = t + tcs;
                    p->waitTime += (tcs / 2);

                    const char* rem_str = (alpha == -1.0) ? "remaining time" : "predicted remaining time";

                    // Preemptions must always print
                   printf("time %dms: Process %s%s arrived; preempting %s (%s %dms) ", 
                           t, p->id, tauStr, processSwappingOut->id, rem_str, processSwappingOut->predictedRemainingTime);

                    printQueue(q);
                } else {
                    if (is_fcfs || is_rr) pushBack(q, p);
                    else insertSorted(q, p, is_srt);

                    if (t < 10000) {
                        printf("time %dms: Process %s%s arrived; added to ready queue ", t, p->id, tauStr);
                        printQueue(q);
                    }
                }
            }
        }

        if (cpuProcess == NULL && processSwappingIn == NULL && processSwappingOut == NULL && !isEmpty(q)) {
            processSwappingIn = popFront(q);
            processSwappingIn->waitTime += (t - processSwappingIn->timeEnteredReadyQueue);
            timeSwapInCompletes = t + (tcs / 2);
        }

        t++; 
    }

    freeQueue(q);

    // writing everything to simout.txt
    int cpu_total_time = 0, total_turnaround = 0, total_wait = 0, total_cs = 0, total_preempts = 0;
    int cpu_turnaround = 0, cpu_wait = 0, cpu_cs = 0, cpu_preempts = 0;
    int io_turnaround = 0, io_wait = 0, io_cs = 0, io_preempts = 0;
    
    int cpu_bound_count = 0, io_bound_count = 0;
    int rr_cpu_hit = 0, rr_io_hit = 0;

    for (int i = 0; i < n; i++) {
        cpu_total_time += procs[i].totalCpuTime;
        total_turnaround += procs[i].turnaroundTime;
        total_wait += procs[i].waitTime;
        total_cs += procs[i].contextSwitches;
        total_preempts += procs[i].preemptions;

        if (procs[i].isCpuBound) {
            cpu_turnaround += procs[i].turnaroundTime;
            cpu_wait += procs[i].waitTime;
            cpu_cs += procs[i].contextSwitches;
            cpu_preempts += procs[i].preemptions;
            cpu_bound_count += procs[i].numCpuBursts;
            rr_cpu_hit += procs[i].rrBurstsCompletedInSlice;
        } else {
            io_turnaround += procs[i].turnaroundTime;
            io_wait += procs[i].waitTime;
            io_cs += procs[i].contextSwitches;
            io_preempts += procs[i].preemptions;
            io_bound_count += procs[i].numCpuBursts;
            rr_io_hit += procs[i].rrBurstsCompletedInSlice;
        }
    }
    
    int total_bursts = cpu_bound_count + io_bound_count;
    double cpu_util = ((double)cpu_total_time / (t-1)) * 100.0; // t instead of t-1 because we break immediately at the correct t now

    fprintf(simout, "Algorithm %s\n", algo);
    fprintf(simout, "-- CPU utilization: %.2f%%\n", cpu_util);
    fprintf(simout, "-- CPU-bound average wait time: %.2f ms\n", cpu_bound_count ? (double)cpu_wait / cpu_bound_count : 0.0);
    fprintf(simout, "-- I/O-bound average wait time: %.2f ms\n", io_bound_count ? (double)io_wait / io_bound_count : 0.0);
    fprintf(simout, "-- overall average wait time: %.2f ms\n", (double)total_wait / total_bursts);
    fprintf(simout, "-- CPU-bound average turnaround time: %.2f ms\n", cpu_bound_count ? (double)cpu_turnaround / cpu_bound_count : 0.0);
    fprintf(simout, "-- I/O-bound average turnaround time: %.2f ms\n", io_bound_count ? (double)io_turnaround / io_bound_count : 0.0);
    fprintf(simout, "-- overall average turnaround time: %.2f ms\n", (double)total_turnaround / total_bursts);
    fprintf(simout, "-- CPU-bound number of context switches: %d\n", cpu_cs);
    fprintf(simout, "-- I/O-bound number of context switches: %d\n", io_cs);
    fprintf(simout, "-- overall number of context switches: %d\n", total_cs);
    fprintf(simout, "-- CPU-bound number of preemptions: %d\n", cpu_preempts);
    fprintf(simout, "-- I/O-bound number of preemptions: %d\n", io_preempts);
    fprintf(simout, "-- overall number of preemptions: %d\n", total_preempts);

    if (is_rr) {
        fprintf(simout, "-- CPU-bound percentage of CPU bursts completed within one time slice: %.2f%%\n", 
                cpu_bound_count ? ((double)rr_cpu_hit / cpu_bound_count) * 100.0 : 0.0);
        fprintf(simout, "-- I/O-bound percentage of CPU bursts completed within one time slice: %.2f%%\n", 
                io_bound_count ? ((double)rr_io_hit / io_bound_count) * 100.0 : 0.0);
        fprintf(simout, "-- overall percentage of CPU bursts completed within one time slice: %.2f%%", 
                ((double)(rr_cpu_hit + rr_io_hit) / total_bursts) * 100.0);
    } else{
        fprintf(simout, "\n");
    }
}


int main(int argc, char ** argv) {
    if (argc != 9) {
        fprintf(stderr, "ERROR: invalid number of arguments\n");
        return EXIT_FAILURE;
    }

    int n = atoi(argv[1]);
    int ncpu = atoi(argv[2]);
    int seed = atoi(argv[3]);
    double lambda = atof(argv[4]);
    double upper = atof(argv[5]);
    int tcs = atoi(argv[6]);
    float alpha = atof(argv[7]);
    int timeSlice = atoi(argv[8]);

    // Error Validation Block
    if (n <= 0) { fprintf(stderr, "ERROR: number of processes must be > 0\n"); return EXIT_FAILURE; }
    if (n > 260) { fprintf(stderr, "ERROR: number of processes must be <= 260\n"); return EXIT_FAILURE; }
    if (ncpu < 0 || ncpu > n) { fprintf(stderr, "ERROR: invalid number of CPU-bound processes\n"); return EXIT_FAILURE; }
    if (lambda <= 0.0) { fprintf(stderr, "ERROR: lambda must be > 0\n"); return EXIT_FAILURE; }
    if (upper <= 0.0) { fprintf(stderr, "ERROR: upper bound must be > 0\n"); return EXIT_FAILURE; }
    if (tcs <= 0 || tcs % 2 != 0) { fprintf(stderr, "ERROR: tcs much be a postive even integer\n"); return EXIT_FAILURE; }
    if ((alpha < 0.0 || alpha > 1.0) && alpha != -1.0) { fprintf(stderr, "ERROR: alpha must be between 0 and 1\n"); return EXIT_FAILURE; }
    if (timeSlice <= 0) { fprintf(stderr, "ERROR: time slice must be > 0\n"); return EXIT_FAILURE; }

    srand48(seed);
    Process* process = generateProcess(n, ncpu, lambda, upper);
    
    outputProcesses(process, n, ncpu, seed, lambda, upper, tcs, alpha, timeSlice);

    FILE* simout = fopen("simout.txt", "w");
    if (!simout) {
        fprintf(stderr, "ERROR: Failed to open simout.txt\n");
        return EXIT_FAILURE;
    }
    
    long long cpu_burst_sum_cpu = 0, cpu_burst_sum_io = 0;
    long long io_burst_sum_cpu = 0, io_burst_sum_io = 0;
    int c_count = 0, i_count = 0, c_io_count = 0, i_io_count = 0;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < process[i].numCpuBursts; j++) {
            if (process[i].isCpuBound) { cpu_burst_sum_cpu += process[i].cpuBursts[j]; c_count++; }
            else { cpu_burst_sum_io += process[i].cpuBursts[j]; i_count++; }
        }
        for (int j = 0; j < process[i].numCpuBursts - 1; j++) {
            if (process[i].isCpuBound) { io_burst_sum_cpu += process[i].ioBursts[j]; c_io_count++; }
            else { io_burst_sum_io += process[i].ioBursts[j]; i_io_count++; }
        }
    }

    fprintf(simout, "-- number of processes: %d\n", n);
    fprintf(simout, "-- number of CPU-bound processes: %d\n", ncpu);
    fprintf(simout, "-- number of I/O-bound processes: %d\n", n - ncpu);
    fprintf(simout, "-- CPU-bound average CPU burst time: %.2f ms\n", c_count ? (double)cpu_burst_sum_cpu / c_count : 0.0);
    fprintf(simout, "-- I/O-bound average CPU burst time: %.2f ms\n", i_count ? (double)cpu_burst_sum_io / i_count : 0.0);
    fprintf(simout, "-- overall average CPU burst time: %.2f ms\n", (double)(cpu_burst_sum_cpu + cpu_burst_sum_io) / (c_count + i_count));
    fprintf(simout, "-- CPU-bound average I/O burst time: %.2f ms\n", c_io_count ? (double)io_burst_sum_cpu / c_io_count : 0.0);
    fprintf(simout, "-- I/O-bound average I/O burst time: %.2f ms\n", i_io_count ? (double)io_burst_sum_io / i_io_count : 0.0);
    fprintf(simout, "-- overall average I/O burst time: %.2f ms\n\n", (double)(io_burst_sum_cpu + io_burst_sum_io) / (c_io_count + i_io_count));

    printf("\n<<< PROJECT SIMULATIONS\n\n");

    runSimulation(process, n, "FCFS", tcs, alpha, timeSlice, lambda, simout);
    printf("\n");
    runSimulation(process, n, alpha == -1.0 ? "SJF-OPT" : "SJF", tcs, alpha, timeSlice, lambda, simout);
    printf("\n");
    runSimulation(process, n, alpha == -1.0 ? "SRT-OPT" : "SRT", tcs, alpha, timeSlice, lambda, simout);
    printf("\n");
    runSimulation(process, n, "RR", tcs, alpha, timeSlice, lambda, simout);

    fclose(simout);

    for(int i = 0; i < n; i++) {
        free(process[i].cpuBursts);
        free(process[i].ioBursts);
    }
    free(process);

    return EXIT_SUCCESS;
}
