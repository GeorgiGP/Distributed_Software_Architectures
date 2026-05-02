/**
 * P2P Динамично балансиране с декомпозиция на Фибоначи
 *
 * Разбива fib(n) на подзадачи, които се разпределят между процесите.
 * Истински паралелизъм - работата се разделя, не само задачите.
 *
 * Компилация: mpicc -O2 -o p2p_fib_decomp p2p_fib_decomposition.c
 * Изпълнение: mpirun -np 4 ./p2p_fib_decomp 45
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#define MAX_QUEUE_SIZE 4096
#define MAX_PARTIAL 2048
int THRESHOLD = 30;  /* Под този праг - изчисляваме директно */

#define TAG_TASK          100
#define TAG_RESULT        101
#define TAG_WORK_REQUEST  102
#define TAG_WORK_RESPONSE 103
#define TAG_TERMINATE     104
#define TAG_FINAL_RESULT  105

/* Задача за изчисление */
typedef struct {
    int n;            /* Числото за изчисление */
    int task_id;      /* Уникален ID */
    int parent_id;    /* ID на родителя (-1 за корен) */
    int parent_rank;  /* Кой процес държи родителя */
    int is_left;      /* 1 = ляв (n-1), 0 = десен (n-2) */
} Task;

/* Частичен резултат - чака две деца */
typedef struct {
    int task_id;
    int n;
    long long left_result;
    long long right_result;
    int left_done;
    int right_done;
    int parent_id;
    int parent_rank;
    int is_left;
    int active;
} PartialResult;

/* Съобщение с резултат */
typedef struct {
    int parent_id;
    int is_left;
    long long value;
} ResultMsg;

/* Опашка от задачи */
typedef struct {
    Task tasks[MAX_QUEUE_SIZE];
    int front, rear, count;
} TaskQueue;

void queue_init(TaskQueue *q) {
    q->front = q->rear = q->count = 0;
}

int queue_empty(TaskQueue *q) {
    return q->count == 0;
}

int queue_size(TaskQueue *q) {
    return q->count;
}

void queue_push(TaskQueue *q, Task t) {
    if (q->count < MAX_QUEUE_SIZE) {
        q->tasks[q->rear] = t;
        q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
        q->count++;
    }
}

Task queue_pop(TaskQueue *q) {
    Task t = {-1, -1, -1, -1, 0};
    if (q->count > 0) {
        t = q->tasks[q->front];
        q->front = (q->front + 1) % MAX_QUEUE_SIZE;
        q->count--;
    }
    return t;
}

/* Директно изчисление (под прага) */
long long fibonacci_direct(int n) {
    if (n <= 1) return n;
    return fibonacci_direct(n - 1) + fibonacci_direct(n - 2);
}

/* Хранилище за частични резултати */
PartialResult partials[MAX_PARTIAL];
int partial_count = 0;
int next_task_id = 0;

int find_partial(int task_id) {
    for (int i = 0; i < partial_count; i++) {
        if (partials[i].active && partials[i].task_id == task_id) {
            return i;
        }
    }
    return -1;
}

int add_partial(int task_id, int n, int parent_id, int parent_rank, int is_left) {
    if (partial_count >= MAX_PARTIAL) return -1;
    int idx = partial_count++;
    partials[idx].task_id = task_id;
    partials[idx].n = n;
    partials[idx].left_result = 0;
    partials[idx].right_result = 0;
    partials[idx].left_done = 0;
    partials[idx].right_done = 0;
    partials[idx].parent_id = parent_id;
    partials[idx].parent_rank = parent_rank;
    partials[idx].is_left = is_left;
    partials[idx].active = 1;
    return idx;
}

/* MPI типове за структурите */
MPI_Datatype MPI_TASK_TYPE;
MPI_Datatype MPI_RESULT_TYPE;

void create_mpi_types() {
    /* Task type */
    int task_blocklens[5] = {1, 1, 1, 1, 1};
    MPI_Aint task_offsets[5];
    task_offsets[0] = offsetof(Task, n);
    task_offsets[1] = offsetof(Task, task_id);
    task_offsets[2] = offsetof(Task, parent_id);
    task_offsets[3] = offsetof(Task, parent_rank);
    task_offsets[4] = offsetof(Task, is_left);
    MPI_Datatype task_types[5] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT};
    MPI_Type_create_struct(5, task_blocklens, task_offsets, task_types, &MPI_TASK_TYPE);
    MPI_Type_commit(&MPI_TASK_TYPE);

    /* Result type */
    int result_blocklens[3] = {1, 1, 1};
    MPI_Aint result_offsets[3];
    result_offsets[0] = offsetof(ResultMsg, parent_id);
    result_offsets[1] = offsetof(ResultMsg, is_left);
    result_offsets[2] = offsetof(ResultMsg, value);
    MPI_Datatype result_types[3] = {MPI_INT, MPI_INT, MPI_LONG_LONG};
    MPI_Type_create_struct(3, result_blocklens, result_offsets, result_types, &MPI_RESULT_TYPE);
    MPI_Type_commit(&MPI_RESULT_TYPE);
}

int main(int argc, char *argv[]) {
    int rank, size;
    int target_fib = 45;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    create_mpi_types();

    if (argc > 2) { THRESHOLD = atoi(argv[2]); } if (argc > 1) {
        target_fib = atoi(argv[1]);
    }

    /* Уникални task_id-та: rank * 1000000 + локален брояч */
    next_task_id = rank * 1000000;

    TaskQueue queue;
    queue_init(&queue);

    /* Само процес 0 започва с началната задача */
    if (rank == 0) {
        Task root = {target_fib, next_task_id++, -1, -1, 0};
        queue_push(&queue, root);
        printf("Изчисление на fib(%d) с %d процеса, праг=%d\n",
               target_fib, size, THRESHOLD);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();

    long long final_result = -1;
    int done = 0;
    int tasks_computed = 0;
    int tasks_split = 0;
    int idle_count = 0;
    int max_idle = 5000;
    int pending_steal = -1;
    int next_victim = (rank + 1) % size;

    while (!done) {
        MPI_Status status;
        int flag;

        /* 1. Проверяваме за входящи задачи */
        MPI_Iprobe(MPI_ANY_SOURCE, TAG_TASK, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            Task t;
            MPI_Recv(&t, 1, MPI_TASK_TYPE, status.MPI_SOURCE, TAG_TASK,
                     MPI_COMM_WORLD, &status);
            queue_push(&queue, t);
        }

        /* 2. Проверяваме за входящи резултати */
        MPI_Iprobe(MPI_ANY_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            ResultMsg res;
            MPI_Recv(&res, 1, MPI_RESULT_TYPE, status.MPI_SOURCE, TAG_RESULT,
                     MPI_COMM_WORLD, &status);

            int idx = find_partial(res.parent_id);
            if (idx >= 0) {
                if (res.is_left) {
                    partials[idx].left_result = res.value;
                    partials[idx].left_done = 1;
                } else {
                    partials[idx].right_result = res.value;
                    partials[idx].right_done = 1;
                }

                /* Ако и двете деца са готови */
                if (partials[idx].left_done && partials[idx].right_done) {
                    long long combined = partials[idx].left_result +
                                         partials[idx].right_result;
                    partials[idx].active = 0;

                    if (partials[idx].parent_id == -1) {
                        /* Това е коренът - имаме финален резултат */
                        final_result = combined;
                        /* Известяваме всички */
                        for (int p = 0; p < size; p++) {
                            if (p != rank) {
                                MPI_Send(&combined, 1, MPI_LONG_LONG, p,
                                         TAG_FINAL_RESULT, MPI_COMM_WORLD);
                            }
                        }
                        done = 1;
                    } else {
                        /* Изпращаме резултат на родителя */
                        ResultMsg parent_res = {
                            partials[idx].parent_id,
                            partials[idx].is_left,
                            combined
                        };
                        MPI_Send(&parent_res, 1, MPI_RESULT_TYPE,
                                 partials[idx].parent_rank, TAG_RESULT,
                                 MPI_COMM_WORLD);
                    }
                }
            }
        }

        /* 3. Проверяваме за финален резултат */
        MPI_Iprobe(MPI_ANY_SOURCE, TAG_FINAL_RESULT, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            MPI_Recv(&final_result, 1, MPI_LONG_LONG, status.MPI_SOURCE,
                     TAG_FINAL_RESULT, MPI_COMM_WORLD, &status);
            done = 1;
        }

        /* 4. Проверяваме за work stealing заявки */
        MPI_Iprobe(MPI_ANY_SOURCE, TAG_WORK_REQUEST, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            int requester;
            MPI_Recv(&requester, 1, MPI_INT, status.MPI_SOURCE, TAG_WORK_REQUEST,
                     MPI_COMM_WORLD, &status);

            Task response = {-1, -1, -1, -1, 0};
            if (queue_size(&queue) > 1) {
                response = queue_pop(&queue);
            }
            MPI_Send(&response, 1, MPI_TASK_TYPE, requester, TAG_WORK_RESPONSE,
                     MPI_COMM_WORLD);
        }

        /* 5. Проверяваме за отговор на наша steal заявка */
        if (pending_steal >= 0) {
            MPI_Iprobe(pending_steal, TAG_WORK_RESPONSE, MPI_COMM_WORLD, &flag, &status);
            if (flag) {
                Task stolen;
                MPI_Recv(&stolen, 1, MPI_TASK_TYPE, pending_steal, TAG_WORK_RESPONSE,
                         MPI_COMM_WORLD, &status);
                if (stolen.n >= 0) {
                    queue_push(&queue, stolen);
                    idle_count = 0;
                }
                pending_steal = -1;
            }
        }

        /* 6. Обработваме задача ако има */
        if (!queue_empty(&queue)) {
            Task t = queue_pop(&queue);
            idle_count = 0;

            if (t.n <= THRESHOLD) {
                /* Директно изчисление */
                long long result = fibonacci_direct(t.n);
                tasks_computed++;

                if (t.parent_id == -1) {
                    /* Коренна задача, но под прага */
                    final_result = result;
                    for (int p = 0; p < size; p++) {
                        if (p != rank) {
                            MPI_Send(&result, 1, MPI_LONG_LONG, p,
                                     TAG_FINAL_RESULT, MPI_COMM_WORLD);
                        }
                    }
                    done = 1;
                } else {
                    /* Изпращаме резултат на родителя */
                    ResultMsg res = {t.parent_id, t.is_left, result};
                    MPI_Send(&res, 1, MPI_RESULT_TYPE, t.parent_rank,
                             TAG_RESULT, MPI_COMM_WORLD);
                }
            } else {
                /* Декомпозиция: създаваме две подзадачи */
                int my_id = next_task_id++;
                add_partial(my_id, t.n, t.parent_id, t.parent_rank, t.is_left);
                tasks_split++;

                /* Ляво дете (n-1) */
                Task left = {t.n - 1, next_task_id++, my_id, rank, 1};
                /* Дясно дете (n-2) */
                Task right = {t.n - 2, next_task_id++, my_id, rank, 0};

                /* Едното оставяме локално, другото даваме на друг процес */
                queue_push(&queue, left);

                if (size > 1) {
                    /* Изпращаме дясното на следващ процес (round-robin) */
                    int target = (rank + 1) % size;
                    MPI_Send(&right, 1, MPI_TASK_TYPE, target, TAG_TASK, MPI_COMM_WORLD);
                } else {
                    queue_push(&queue, right);
                }
            }
        } else if (pending_steal < 0 && size > 1) {
            /* Нямаме работа - опитваме да откраднем */
            if (next_victim == rank) {
                next_victim = (next_victim + 1) % size;
            }
            MPI_Send(&rank, 1, MPI_INT, next_victim, TAG_WORK_REQUEST, MPI_COMM_WORLD);
            pending_steal = next_victim;
            next_victim = (next_victim + 1) % size;
            idle_count++;
        } else {
            idle_count++;
        }

        /* Ограничение за безкраен цикъл (само за single process) */
        if (size == 1 && idle_count > 100 && queue_empty(&queue) && partial_count == 0) {
            done = 1;
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double end_time = MPI_Wtime();

    /* Събираме статистика */
    int total_computed, total_split;
    MPI_Reduce(&tasks_computed, &total_computed, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&tasks_split, &total_split, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    double local_time = end_time - start_time;
    double max_time, min_time;
    MPI_Reduce(&local_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_time, &min_time, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("\n=== Резултати (P2P Fibonacci Decomposition) ===\n");
        printf("fib(%d) = %lld\n", target_fib, final_result);
        printf("Брой процеси: %d\n", size);
        printf("Праг за директно изчисление: %d\n", THRESHOLD);
        printf("Задачи разделени: %d\n", total_split);
        printf("Задачи изчислени директно: %d\n", total_computed);
        printf("Време (max): %.3f секунди\n", max_time);
        printf("Време (min): %.3f секунди\n", min_time);
        printf("Баланс (min/max): %.1f%%\n", (min_time / max_time) * 100);
        printf("================================================\n");
    }

    MPI_Type_free(&MPI_TASK_TYPE);
    MPI_Type_free(&MPI_RESULT_TYPE);
    MPI_Finalize();
    return 0;
}
