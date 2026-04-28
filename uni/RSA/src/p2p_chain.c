/**
 * P2P Динамично балансиране с верижна топология
 *
 * Всеки процес комуникира само със съседите си (ляв и десен).
 * При изчерпване на задачите, поисква работа от съсед.
 *
 * Компилация: mpicc -O2 -o p2p_chain p2p_chain.c
 * Изпълнение: mpirun -np 4 ./p2p_chain 40
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* Получаване на съседите при верижна топология */
void get_neighbors(int rank, int size, int *left, int *right) {
    *left = (rank > 0) ? rank - 1 : -1;
    *right = (rank < size - 1) ? rank + 1 : -1;
}

/* Обработка на входящи заявки за работа */
int handle_work_requests(TaskQueue *q, int rank, int size) {
    MPI_Status status;
    int flag;
    int buffer;

    MPI_Iprobe(MPI_ANY_SOURCE, TAG_WORK_REQUEST, MPI_COMM_WORLD, &flag, &status);

    if (flag) {
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

            printf("Rank %d: Споделих %d задачи с Rank %d\n", rank, to_share, source);
            return to_share;
        } else {
            /* Няма достатъчно работа за споделяне */
            int zero = 0;
            MPI_Send(&zero, 1, MPI_INT, source, TAG_NO_WORK, MPI_COMM_WORLD);
        }
    }

    return 0;
}

/* Поискване на работа от съсед */
int request_work(TaskQueue *q, int neighbor, int rank) {
    if (neighbor < 0) return 0;

    int request = 1;
    MPI_Send(&request, 1, MPI_INT, neighbor, TAG_WORK_REQUEST, MPI_COMM_WORLD);

    MPI_Status status;
    int received_count;
    MPI_Recv(&received_count, 1, MPI_INT, neighbor, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

    if (status.MPI_TAG == TAG_WORK_RESPONSE && received_count > 0) {
        int received_tasks[MAX_QUEUE_SIZE];
        MPI_Recv(received_tasks, received_count, MPI_INT, neighbor, TAG_WORK_RESPONSE, MPI_COMM_WORLD, &status);

        for (int i = 0; i < received_count; i++) {
            queue_push(q, received_tasks[i]);
        }

        printf("Rank %d: Получих %d задачи от Rank %d\n", rank, received_count, neighbor);
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

    if (argc > 1) {
        base_fib = atoi(argv[1]);
    }

    int left_neighbor, right_neighbor;
    get_neighbors(rank, size, &left_neighbor, &right_neighbor);

    printf("Rank %d: Съседи - ляв: %d, десен: %d\n", rank, left_neighbor, right_neighbor);

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
    int max_idle_rounds = size * 2;

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

            /* Периодично обработваме заявки */
            if (tasks_processed % 2 == 0) {
                handle_work_requests(&queue, rank, size);
            }
        } else {
            /* Няма работа - поискваме от съседите */
            int got_work = 0;

            /* Първо питаме левия съсед */
            if (left_neighbor >= 0) {
                got_work = request_work(&queue, left_neighbor, rank);
            }

            /* Ако не получихме работа, питаме десния */
            if (!got_work && right_neighbor >= 0) {
                got_work = request_work(&queue, right_neighbor, rank);
            }

            if (!got_work) {
                idle_rounds++;
            } else {
                idle_rounds = 0;
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

    /* Статистики за всеки процес */
    printf("Rank %d: Обработих %d задачи, локална сума: %lld\n",
           rank, tasks_processed, local_sum);

    if (rank == 0) {
        printf("\n=== Резултати (P2P Chain Topology) ===\n");
        printf("Брой процеси: %d\n", size);
        printf("Топология: Верига\n");
        printf("Брой обработени задачи: %d\n", total_tasks);
        printf("Обща сума: %lld\n", global_sum);
        printf("Време за изпълнение: %.3f секунди\n", max_time);
        printf("======================================\n");
    }

    MPI_Finalize();
    return 0;
}
