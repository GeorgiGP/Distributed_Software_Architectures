/**
 * P2P Динамично балансиране с пълен граф (All-to-All)
 *
 * Всеки процес може да комуникира директно с всеки друг.
 * Използва polling-based work stealing без блокиране.
 *
 * Компилация: mpicc -O2 -o p2p_full p2p_full.c
 * Изпълнение: mpirun -np 4 ./p2p_full 40
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mpi.h>

#define MAX_QUEUE_SIZE 256
#define NUM_TASKS 128
#define TAG_WORK_REQUEST  100
#define TAG_WORK_RESPONSE 101
#define TAG_TERMINATE     102

/* Локална опашка от задачи */
typedef struct {
    int tasks[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int count;
} TaskQueue;

void queue_init(TaskQueue *q) {
    q->front = 0;
    q->rear = 0;
    q->count = 0;
}

int queue_empty(TaskQueue *q) {
    return q->count == 0;
}

int queue_size(TaskQueue *q) {
    return q->count;
}

void queue_push(TaskQueue *q, int task) {
    if (q->count < MAX_QUEUE_SIZE) {
        q->tasks[q->rear] = task;
        q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
        q->count++;
    }
}

int queue_pop(TaskQueue *q) {
    if (q->count > 0) {
        int task = q->tasks[q->front];
        q->front = (q->front + 1) % MAX_QUEUE_SIZE;
        q->count--;
        return task;
    }
    return -1;
}

/* Рекурсивно изчисление на число на Фибоначи */
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

    srand(time(NULL) + rank * 1000);

    if (argc > 1) {
        base_fib = atoi(argv[1]);
    }

    /* Инициализация на локалната опашка */
    TaskQueue queue;
    queue_init(&queue);

    /* Разпределение на първоначалните задачи - НЕХОМОГЕННО */
    /* Процес 0 получава всички тежки задачи, останалите - леки */
    int tasks[NUM_TASKS];
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
    MPI_Bcast(tasks, NUM_TASKS, MPI_INT, 0, MPI_COMM_WORLD);

    /* НЕХОМОГЕННО разпределение: процес 0 взима тежките задачи */
    if (size == 1) {
        for (int i = 0; i < NUM_TASKS; i++) {
            queue_push(&queue, tasks[i]);
        }
    } else {
        int heavy_per_proc = (NUM_TASKS / 2) / size;
        int light_per_proc = (NUM_TASKS / 2) / size;

        /* Процес 0 получава 75% от тежките задачи */
        if (rank == 0) {
            int heavy_count = (NUM_TASKS / 2) * 3 / 4;  /* 75% от тежките */
            for (int i = 0; i < heavy_count; i++) {
                queue_push(&queue, tasks[i]);
            }
        } else {
            /* Останалите процеси делят останалите задачи */
            int start_heavy = (NUM_TASKS / 2) * 3 / 4;
            int remaining_heavy = (NUM_TASKS / 2) - start_heavy;
            int light_start = NUM_TASKS / 2;

            /* Всеки процес взима равен дял от останалите */
            for (int i = rank - 1; i < remaining_heavy; i += (size - 1)) {
                queue_push(&queue, tasks[start_heavy + i]);
            }
            for (int i = rank - 1; i < NUM_TASKS / 2; i += (size - 1)) {
                queue_push(&queue, tasks[light_start + i]);
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();

    /* Главен цикъл */
    long long local_sum = 0;
    int tasks_processed = 0;
    int done_count = 0;
    int my_done = 0;
    int idle_iterations = 0;
    int max_idle = 1000;

    /* За work stealing */
    int pending_request_to = -1;
    int next_target = (rank + 1) % size;

    while (done_count < size) {
        MPI_Status status;
        int flag;

        /* 1. Обработваме входящи заявки за работа */
        MPI_Iprobe(MPI_ANY_SOURCE, TAG_WORK_REQUEST, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            int source = status.MPI_SOURCE;
            int dummy;
            MPI_Recv(&dummy, 1, MPI_INT, source, TAG_WORK_REQUEST, MPI_COMM_WORLD, &status);

            int response = (queue_size(&queue) > 1) ? queue_pop(&queue) : -1;
            MPI_Send(&response, 1, MPI_INT, source, TAG_WORK_RESPONSE, MPI_COMM_WORLD);
        }

        /* 2. Обработваме входящи DONE съобщения */
        MPI_Iprobe(MPI_ANY_SOURCE, TAG_TERMINATE, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            int dummy;
            MPI_Recv(&dummy, 1, MPI_INT, status.MPI_SOURCE, TAG_TERMINATE, MPI_COMM_WORLD, &status);
            done_count++;
        }

        /* 3. Проверяваме за отговор на наша заявка */
        if (pending_request_to >= 0) {
            MPI_Iprobe(pending_request_to, TAG_WORK_RESPONSE, MPI_COMM_WORLD, &flag, &status);
            if (flag) {
                int response;
                MPI_Recv(&response, 1, MPI_INT, pending_request_to, TAG_WORK_RESPONSE, MPI_COMM_WORLD, &status);
                if (response >= 0) {
                    queue_push(&queue, response);
                    idle_iterations = 0;
                }
                pending_request_to = -1;
            }
        }

        /* 4. Изпълняваме задача ако има */
        if (!queue_empty(&queue)) {
            int task = queue_pop(&queue);
            long long result = fibonacci(task);
            local_sum += result;
            tasks_processed++;
            idle_iterations = 0;
            my_done = 0;
        } else if (pending_request_to < 0 && !my_done) {
            /* 5. Нямаме работа - пращаме заявка на следващ процес (round-robin) */
            if (size > 1) {
                /* Пълен граф - можем да питаме всеки */
                if (next_target == rank) {
                    next_target = (next_target + 1) % size;
                }

                int dummy = rank;
                MPI_Send(&dummy, 1, MPI_INT, next_target, TAG_WORK_REQUEST, MPI_COMM_WORLD);
                pending_request_to = next_target;
                next_target = (next_target + 1) % size;
            }

            idle_iterations++;

            /* Ако сме idle твърде дълго, сигнализираме DONE */
            if (idle_iterations > max_idle && !my_done) {
                my_done = 1;
                done_count++;
                /* Broadcast DONE на всички */
                for (int p = 0; p < size; p++) {
                    if (p != rank) {
                        int dummy = rank;
                        MPI_Send(&dummy, 1, MPI_INT, p, TAG_TERMINATE, MPI_COMM_WORLD);
                    }
                }
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
        printf("\n=== Резултати (P2P Full Graph) ===\n");
        printf("Брой процеси: %d\n", size);
        printf("Брой задачи: %d\n", total_tasks);
        printf("Обща сума: %lld\n", global_sum);
        printf("Време за изпълнение: %.3f секунди\n", max_time);
        printf("==================================\n");
    }

    MPI_Finalize();
    return 0;
}
