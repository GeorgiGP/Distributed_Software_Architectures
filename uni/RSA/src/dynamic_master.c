/**
 * Динамично балансиране с централизиран координатор (Master-Worker)
 *
 * Master (rank 0) раздава задачите динамично.
 * Workers искат нова задача когато приключат с текущата.
 *
 * Компилация: mpicc -O2 -o dynamic_master dynamic_master.c
 * Изпълнение: mpirun -np 4 ./dynamic_master 40
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#define NUM_TASKS 32
#define TAG_WORK_REQUEST 100
#define TAG_WORK_SEND    101
#define TAG_RESULT       102
#define TAG_TERMINATE    103

/* Рекурсивно изчисление на число на Фибоначи */
long long fibonacci(int n) {
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

void master(int size, int base_fib) {
    int tasks[NUM_TASKS];
    int next_task = 0;
    int completed = 0;
    long long total_sum = 0;

    /* Генериране на задачи */
    for (int i = 0; i < NUM_TASKS; i++) {
        tasks[i] = base_fib - 5 + (i % 11);
    }

    /* Раздаване на първоначални задачи */
    for (int worker = 1; worker < size && next_task < NUM_TASKS; worker++) {
        MPI_Send(&tasks[next_task], 1, MPI_INT, worker, TAG_WORK_SEND, MPI_COMM_WORLD);
        next_task++;
    }

    /* Цикъл на обработка */
    while (completed < NUM_TASKS) {
        MPI_Status status;
        long long result;
        MPI_Recv(&result, 1, MPI_LONG_LONG, MPI_ANY_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &status);
        total_sum += result;
        completed++;

        int worker = status.MPI_SOURCE;

        if (next_task < NUM_TASKS) {
            MPI_Send(&tasks[next_task], 1, MPI_INT, worker, TAG_WORK_SEND, MPI_COMM_WORLD);
            next_task++;
        } else {
            int term = -1;
            MPI_Send(&term, 1, MPI_INT, worker, TAG_TERMINATE, MPI_COMM_WORLD);
        }
    }

    /* Край на останалите workers */
    for (int worker = 1; worker < size; worker++) {
        int term = -1;
        MPI_Send(&term, 1, MPI_INT, worker, TAG_TERMINATE, MPI_COMM_WORLD);
    }

    printf("Обща сума: %lld\n", total_sum);
}

void worker() {
    while (1) {
        MPI_Status status;
        int task;
        MPI_Recv(&task, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == TAG_TERMINATE) {
            break;
        }

        long long result = fibonacci(task);
        MPI_Send(&result, 1, MPI_LONG_LONG, 0, TAG_RESULT, MPI_COMM_WORLD);
    }
}

int main(int argc, char *argv[]) {
    int rank, size;
    int base_fib = 40;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc > 1) {
        base_fib = atoi(argv[1]);
    }

    if (size < 2) {
        if (rank == 0) {
            printf("Нужни са поне 2 процеса за Master-Worker\n");
            /* Fallback to sequential */
            double start = MPI_Wtime();
            long long sum = 0;
            for (int i = 0; i < NUM_TASKS; i++) {
                sum += fibonacci(base_fib - 5 + (i % 11));
            }
            double end = MPI_Wtime();
            printf("\n=== Резултати (Динамично Master-Worker) ===\n");
            printf("Брой процеси: %d\n", size);
            printf("Брой задачи: %d\n", NUM_TASKS);
            printf("Обща сума: %lld\n", sum);
            printf("Време за изпълнение: %.3f секунди\n", end - start);
            printf("============================================\n");
        }
        MPI_Finalize();
        return 0;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();

    if (rank == 0) {
        master(size, base_fib);
    } else {
        worker();
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double end_time = MPI_Wtime();

    if (rank == 0) {
        printf("\n=== Резултати (Динамично Master-Worker) ===\n");
        printf("Брой процеси: %d\n", size);
        printf("Брой задачи: %d\n", NUM_TASKS);
        printf("Време за изпълнение: %.3f секунди\n", end_time - start_time);
        printf("============================================\n");
    }

    MPI_Finalize();
    return 0;
}
