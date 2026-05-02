/**
 * Статично циклично балансиране при паралелно изчисление
 *
 * Изчислява числата на Фибоначи рекурсивно - задача с нехомогенно натоварване.
 * Използва се като базова линия за сравнение с динамичното балансиране.
 *
 * Компилация: mpicc -O2 -o static_balancing static_balancing.c
 * Изпълнение: mpirun -np 4 ./static_balancing 40
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#define NUM_TASKS 128

/* Рекурсивно изчисление на число на Фибоначи - умишлено неоптимално за демонстрация */
long long fibonacci(int n) {
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
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

    /* Масив със задачи - НЕХОМОГЕННО натоварване */
    int tasks[NUM_TASKS];

    /* Rank 0 генерира задачите */
    if (rank == 0) {
        /* Тежки задачи: fib(42-45), леки: fib(35-38) */
        for (int i = 0; i < NUM_TASKS; i++) {
            if (i < NUM_TASKS / 2) {
                tasks[i] = base_fib - 8 + (i % 4);  /* 42, 43, 44, 45 */
            } else {
                tasks[i] = base_fib - 12 + (i % 4);  /* 35, 36, 37, 38 */
            }
        }
    }

    /* Разпращане на задачите до всички процеси */
    MPI_Bcast(tasks, NUM_TASKS, MPI_INT, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();

    /* Статично НЕХОМОГЕННО разпределение: процес 0 получава тежките задачи */
    long long local_sum = 0;
    int tasks_processed = 0;

    if (size == 1) {
        for (int i = 0; i < NUM_TASKS; i++) {
            long long result = fibonacci(tasks[i]);
            local_sum += result;
            tasks_processed++;
        }
    } else {
        /* Процес 0 получава 75% от тежките задачи */
        if (rank == 0) {
            int heavy_count = (NUM_TASKS / 2) * 3 / 4;
            for (int i = 0; i < heavy_count; i++) {
                long long result = fibonacci(tasks[i]);
                local_sum += result;
                tasks_processed++;
            }
        } else {
            /* Останалите процеси делят останалите задачи */
            int start_heavy = (NUM_TASKS / 2) * 3 / 4;
            int remaining_heavy = (NUM_TASKS / 2) - start_heavy;
            int light_start = NUM_TASKS / 2;

            for (int i = rank - 1; i < remaining_heavy; i += (size - 1)) {
                long long result = fibonacci(tasks[start_heavy + i]);
                local_sum += result;
                tasks_processed++;
            }
            for (int i = rank - 1; i < NUM_TASKS / 2; i += (size - 1)) {
                long long result = fibonacci(tasks[light_start + i]);
                local_sum += result;
                tasks_processed++;
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double end_time = MPI_Wtime();

    /* Събиране на резултатите */
    long long global_sum;
    int total_tasks;
    MPI_Reduce(&local_sum, &global_sum, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&tasks_processed, &total_tasks, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    double local_time = end_time - start_time;
    double max_time;
    MPI_Reduce(&local_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("\n=== Резултати (Статично балансиране) ===\n");
        printf("Брой процеси: %d\n", size);
        printf("Брой задачи: %d\n", total_tasks);
        printf("Обща сума: %lld\n", global_sum);
        printf("Време за изпълнение: %.3f секунди\n", max_time);
        printf("========================================\n");
    }

    MPI_Finalize();
    return 0;
}
