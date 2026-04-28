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

/* Рекурсивно изчисление на число на Фибоначи - умишлено неоптимално за демонстрация */
long long fibonacci(int n) {
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main(int argc, char *argv[]) {
    int rank, size;
    int base_fib = 40;  /* Базова стойност за Фибоначи */
    int num_tasks = 16; /* Брой задачи за разпределение */

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc > 1) {
        base_fib = atoi(argv[1]);
    }

    /* Генериране на задачи с различна сложност */
    int *tasks = NULL;
    if (rank == 0) {
        tasks = (int *)malloc(num_tasks * sizeof(int));
        for (int i = 0; i < num_tasks; i++) {
            /* Вариация в сложността: base_fib - 5 до base_fib + 5 */
            tasks[i] = base_fib - 5 + (i % 11);
        }
    }

    /* Разпределение на задачите към всички процеси */
    int *local_tasks = (int *)malloc(num_tasks * sizeof(int));
    MPI_Bcast(tasks, num_tasks, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank == 0) {
        for (int i = 0; i < num_tasks; i++) {
            local_tasks[i] = tasks[i];
        }
    } else {
        MPI_Bcast(local_tasks, num_tasks, MPI_INT, 0, MPI_COMM_WORLD);
    }

    /* Всъщност използваме scatter на задачите */
    if (rank == 0) {
        for (int i = 0; i < num_tasks; i++) {
            local_tasks[i] = tasks[i];
        }
    }
    MPI_Bcast(local_tasks, num_tasks, MPI_INT, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();

    /* Статично циклично балансиране: процес i обработва задачи i, i+size, i+2*size, ... */
    long long local_sum = 0;
    int tasks_processed = 0;

    for (int i = rank; i < num_tasks; i += size) {
        long long result = fibonacci(local_tasks[i]);
        local_sum += result;
        tasks_processed++;

        if (rank == 0) {
            printf("Rank %d: Задача %d (Fib(%d)) = %lld\n",
                   rank, i, local_tasks[i], result);
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

    free(local_tasks);
    if (rank == 0) free(tasks);

    MPI_Finalize();
    return 0;
}
