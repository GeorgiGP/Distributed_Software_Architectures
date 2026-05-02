#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

int THRESHOLD = 30;

long long fib(int n) {
    if (n <= 1) return n;
    return fib(n-1) + fib(n-2);
}

int count_tasks(int n) {
    if (n <= THRESHOLD) return 1;
    return 1 + count_tasks(n-1) + count_tasks(n-2);
}

long long fib_parallel(int n, int rank, int size, int* tasks_done) {
    if (n <= THRESHOLD) {
        (*tasks_done)++;
        return fib(n);
    }
    (*tasks_done)++;
    return fib_parallel(n-1, rank, size, tasks_done) + fib_parallel(n-2, rank, size, tasks_done);
}

int main(int argc, char *argv[]) {
    int rank, size;
    int target = 45;
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (argc > 1) target = atoi(argv[1]);
    if (argc > 2) THRESHOLD = atoi(argv[2]);
    
    int tasks = count_tasks(target);
    
    double start = MPI_Wtime();
    
    // Simple work distribution - each process computes subset
    long long local_sum = 0;
    int local_tasks = 0;
    
    // Decompose at top level
    if (target > THRESHOLD) {
        int work_items[100];
        int work_count = 0;
        
        // Generate work at threshold+5 level
        int decomp_level = THRESHOLD + 5;
        if (decomp_level > target) decomp_level = target;
        
        // BFS to find all nodes at decomp_level
        work_items[work_count++] = target;
        int processed = 0;
        while (processed < work_count) {
            int n = work_items[processed];
            if (n <= decomp_level || work_count >= 90) {
                processed++;
                continue;
            }
            // Replace with children
            work_items[processed] = n - 1;
            work_items[work_count++] = n - 2;
        }
        
        // Each process takes its share
        for (int i = rank; i < work_count; i += size) {
            local_sum += fib(work_items[i]);
            local_tasks++;
        }
    } else {
        if (rank == 0) {
            local_sum = fib(target);
            local_tasks = 1;
        }
    }
    
    long long total_sum;
    MPI_Reduce(&local_sum, &total_sum, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    
    double end = MPI_Wtime();
    double local_time = end - start;
    double max_time;
    MPI_Reduce(&local_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    
    if (rank == 0) {
        printf("Праг=%d, Задачи=%d, Време=%.3f s, fib(%d)=%lld\n", 
               THRESHOLD, tasks, max_time, target, total_sum);
    }
    
    MPI_Finalize();
    return 0;
}
