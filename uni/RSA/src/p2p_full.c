/**
 * P2P Динамично балансиране с пълен граф (All-to-All)
 *
 * Всеки процес може да комуникира директно с всеки друг.
 * При изчерпване на задачите, поисква работа от произволен процес.
 *
 * Компилация: mpicc -O2 -o p2p_full p2p_full.c
 * Изпълнение: mpirun -np 4 ./p2p_full 40
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mpi.h>

#define MAX_QUEUE_SIZE 1024
#define TAG_WORK_REQUEST  100
#define TAG_WORK_RESPONSE 101
#define TAG_TERMINATE     102
#define TAG_NO_WORK       103

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

/* Избор на произволен процес за поискване на работа */
int choose_random_peer(int rank, int size, int *tried, int num_tried) {
    if (num_tried >= size - 1) return -1;

    int peer;
    int found;

    do {
        peer = rand() % size;
        if (peer == rank) continue;

        found = 0;
        for (int i = 0; i < num_tried; i++) {
            if (tried[i] == peer) {
                found = 1;
                break;
            }
        }
    } while (peer == rank || found);

    return peer;
}

/* Обработка на входящи заявки за работа - неблокираща */
int handle_work_requests(TaskQueue *q, int rank, int size) {
    MPI_Status status;
    int flag;
    int buffer;
    int handled = 0;

    /* Проверка за входящи заявки от всички процеси */
    while (1) {
        MPI_Iprobe(MPI_ANY_SOURCE, TAG_WORK_REQUEST, MPI_COMM_WORLD, &flag, &status);

        if (!flag) break;

        int source = status.MPI_SOURCE;
        MPI_Recv(&buffer, 1, MPI_INT, source, TAG_WORK_REQUEST, MPI_COMM_WORLD, &status);

        if (queue_size(q) > 1) {
            /* Споделяме половината от задачите */
            int to_share = queue_size(q) / 2;
            int shared_tasks[MAX_QUEUE_SIZE];

            for (int i = 0; i < to_share; i++) {
                shared_tasks[i] = queue_pop(q);
            }

            /* Изпращане на броя задачи и самите задачи */
            MPI_Send(&to_share, 1, MPI_INT, source, TAG_WORK_RESPONSE, MPI_COMM_WORLD);
            MPI_Send(shared_tasks, to_share, MPI_INT, source, TAG_WORK_RESPONSE, MPI_COMM_WORLD);

            printf("Rank %d: Споделих %d задачи с Rank %d (Full Graph)\n", rank, to_share, source);
            handled++;
        } else {
            /* Няма достатъчно работа за споделяне */
            int zero = 0;
            MPI_Send(&zero, 1, MPI_INT, source, TAG_NO_WORK, MPI_COMM_WORLD);
        }
    }

    return handled;
}

/* Поискване на работа от процес */
int request_work_from(TaskQueue *q, int peer, int rank) {
    if (peer < 0) return 0;

    int request = 1;
    MPI_Send(&request, 1, MPI_INT, peer, TAG_WORK_REQUEST, MPI_COMM_WORLD);

    MPI_Status status;
    int received_count;
    MPI_Recv(&received_count, 1, MPI_INT, peer, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

    if (status.MPI_TAG == TAG_WORK_RESPONSE && received_count > 0) {
        int received_tasks[MAX_QUEUE_SIZE];
        MPI_Recv(received_tasks, received_count, MPI_INT, peer, TAG_WORK_RESPONSE, MPI_COMM_WORLD, &status);

        for (int i = 0; i < received_count; i++) {
            queue_push(q, received_tasks[i]);
        }

        printf("Rank %d: Получих %d задачи от Rank %d (Full Graph)\n", rank, received_count, peer);
        return received_count;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int rank, size;
    int base_fib = 40;
    int num_tasks = 32;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* Инициализация на генератор на случайни числа */
    srand(time(NULL) + rank);

    if (argc > 1) {
        base_fib = atoi(argv[1]);
    }

    printf("Rank %d: Топология - пълен граф (всеки към всеки)\n", rank);

    /* Инициализация на локалната опашка */
    TaskQueue queue;
    queue_init(&queue);

    /* Rank 0 генерира и разпределя първоначалните задачи */
    if (rank == 0) {
        for (int i = 0; i < num_tasks; i++) {
            int task = base_fib - 5 + (i % 11);
            int target_rank = i % size;

            if (target_rank == 0) {
                queue_push(&queue, task);
            } else {
                MPI_Send(&task, 1, MPI_INT, target_rank, TAG_WORK_RESPONSE, MPI_COMM_WORLD);
            }
        }

        /* Сигнализираме край на първоначалното разпределение */
        int end_signal = -1;
        for (int r = 1; r < size; r++) {
            MPI_Send(&end_signal, 1, MPI_INT, r, TAG_WORK_RESPONSE, MPI_COMM_WORLD);
        }
    } else {
        /* Получаване на първоначалните задачи */
        int task;
        MPI_Status status;
        while (1) {
            MPI_Recv(&task, 1, MPI_INT, 0, TAG_WORK_RESPONSE, MPI_COMM_WORLD, &status);
            if (task == -1) break;
            queue_push(&queue, task);
        }
    }

    printf("Rank %d: Започвам с %d задачи\n", rank, queue_size(&queue));

    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();

    /* Главен цикъл на обработка */
    long long local_sum = 0;
    int tasks_processed = 0;
    int idle_rounds = 0;
    int max_idle_rounds = size * 3;

    /* Масив за проследяване на вече питаните процеси в текущия "stealing round" */
    int *tried_peers = (int *)malloc(size * sizeof(int));
    int num_tried = 0;

    while (idle_rounds < max_idle_rounds) {
        /* Обработка на входящи заявки */
        handle_work_requests(&queue, rank, size);

        if (!queue_empty(&queue)) {
            /* Има работа - изпълняваме задача */
            int task = queue_pop(&queue);
            long long result = fibonacci(task);
            local_sum += result;
            tasks_processed++;
            idle_rounds = 0;
            num_tried = 0;  /* Reset на списъка с питани процеси */

            /* Периодично обработваме заявки */
            if (tasks_processed % 2 == 0) {
                handle_work_requests(&queue, rank, size);
            }
        } else {
            /* Няма работа - поискваме от произволен процес */
            int peer = choose_random_peer(rank, size, tried_peers, num_tried);

            if (peer >= 0) {
                tried_peers[num_tried++] = peer;
                int got_work = request_work_from(&queue, peer, rank);

                if (got_work) {
                    idle_rounds = 0;
                    num_tried = 0;
                }
            } else {
                /* Питахме всички - увеличаваме idle counter */
                idle_rounds++;
                num_tried = 0;  /* Започваме нов цикъл на питане */
            }
        }
    }

    free(tried_peers);

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

    /* Статистики за всеки процес */
    printf("Rank %d: Обработих %d задачи, локална сума: %lld\n",
           rank, tasks_processed, local_sum);

    if (rank == 0) {
        printf("\n=== Резултати (P2P Full Graph Topology) ===\n");
        printf("Брой процеси: %d\n", size);
        printf("Топология: Пълен граф (All-to-All)\n");
        printf("Брой обработени задачи: %d\n", total_tasks);
        printf("Обща сума: %lld\n", global_sum);
        printf("Време за изпълнение: %.3f секунди\n", max_time);
        printf("============================================\n");
    }

    MPI_Finalize();
    return 0;
}
