#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "backprop.h"
#include "omp.h"

extern char* strcpy();
extern void exit();
//extern int cpu_offset;
int cpu_offset = 0;
double gettime() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec + t.tv_usec * 1e-6;
}

int layer_size = 0;

void backprop_face() {
    BPNN* net;
    int i;
    float out_err, hid_err;
    net = bpnn_create(layer_size, 16, 1);  // (16, 1 can not be changed)

    printf("Input layer size : %d\n", layer_size);
    load(net);
    //entering the training kernel, only one iteration
    printf("Starting training kernel\n");
    double starttime = gettime();
    bpnn_train_kernel(net, &out_err, &hid_err);
    double endtime = gettime();
    printf("CAUTION: time: %lf mseconds\n", 1000.0 * (endtime - starttime));

    bpnn_free(net);
    printf("\nFinish the training for one iteration\n");
}

int setup(int argc, char** argv) {
    int seed;

    if (argc != 3) {
        fprintf(stderr, "usage: backprop <num of input elements>\n");
        exit(0);
    }
    layer_size = atoi(argv[1]);
    cpu_offset = atoi(argv[2]);

    if (layer_size % 16 != 0) {
        fprintf(stderr, "The number of input points must be divided by 16\n");
        exit(0);
    }

    seed = 7;
    bpnn_initialize(seed);
    backprop_face();

    exit(0);
}
