/**
 * P2P Chain топология с декомпозиция на Фибоначи
 *
 * Подобно на p2p_fib_decomposition.c, но work stealing само от съседи.
 *
 * Компилация: mpicc -O2 -o p2p_fib_decomp_chain p2p_fib_decomp_chain.c
 * Изпълнение: mpirun -np 4 ./p2p_fib_decomp_chain 46
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#define MAX_QUEUE_SIZE 4096
#define MAX_PARTIAL 2048
int THRESHOLD = 30;

#define TAG_TASK          100
#define TAG_RESULT        101
#define TAG_WORK_REQUEST  102
#define TAG_WORK_RESPONSE 103
#define TAG_FINAL_RESULT  105

typedef struct {
    int n;
    int task_id;
    int parent_id;
    int parent_rank;
    int is_left;
} Task;

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

typedef struct {
    int parent_id;
    int is_left;
    long long value;
} ResultMsg;

typedef struct {
    Task tasks[MAX_QUEUE_SIZE];
    int front, rear, count;
} TaskQueue;

void queue_init(TaskQueue *q) { q->front = q->rear = q->count = 0; }
int queue_empty(TaskQueue *q) { return q->count == 0; }
int queue_size(TaskQueue *q) { return q->count; }

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

long long fibonacci_direct(int n) {
    if (n <= 1) return n;
    return fibonacci_direct(n - 1) + fibonacci_direct(n - 2);
}

PartialResult partials[MAX_PARTIAL];
int partial_count = 0;
int next_task_id = 0;

int find_partial(int task_id) {
    for (int i = 0; i < partial_count; i++) {
        if (partials[i].active && partials[i].task_id == task_id) return i;
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

MPI_Datatype MPI_TASK_TYPE;
MPI_Datatype MPI_RESULT_TYPE;

void create_mpi_types() {
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
    int target_fib = 46;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    create_mpi_types();

    if (argc > 1) target_fib = atoi(argv[1]);
    if (argc > 2) THRESHOLD = atoi(argv[2]);

    next_task_id = rank * 1000000;

    TaskQueue queue;
    queue_init(&queue);

    /* Chain neighbors */
    int left_neighbor = (rank > 0) ? rank - 1 : -1;
    int right_neighbor = (rank < size - 1) ? rank + 1 : -1;

    if (rank == 0) {
        Task root = {target_fib, next_task_id++, -1, -1, 0};
        queue_push(&queue, root);
        printf("fib(%d) с %d процеса (CHAIN топология), праг=%d\n", target_fib, size, THRESHOLD);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();

    long long final_result = -1;
    int done = 0;
    int tasks_computed = 0;
    int tasks_split = 0;
    int idle_count = 0;
    int pending_steal = -1;
    int steal_from_right = 1;

    while (!done) {
        MPI_Status status;
        int flag;

        MPI_Iprobe(MPI_ANY_SOURCE, TAG_TASK, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            Task t;
            MPI_Recv(&t, 1, MPI_TASK_TYPE, status.MPI_SOURCE, TAG_TASK, MPI_COMM_WORLD, &status);
            queue_push(&queue, t);
        }

        MPI_Iprobe(MPI_ANY_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            ResultMsg res;
            MPI_Recv(&res, 1, MPI_RESULT_TYPE, status.MPI_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &status);
            int idx = find_partial(res.parent_id);
            if (idx >= 0) {
                if (res.is_left) { partials[idx].left_result = res.value; partials[idx].left_done = 1; }
                else { partials[idx].right_result = res.value; partials[idx].right_done = 1; }
                if (partials[idx].left_done && partials[idx].right_done) {
                    long long combined = partials[idx].left_result + partials[idx].right_result;
                    partials[idx].active = 0;
                    if (partials[idx].parent_id == -1) {
                        final_result = combined;
                        for (int p = 0; p < size; p++) if (p != rank) MPI_Send(&combined, 1, MPI_LONG_LONG, p, TAG_FINAL_RESULT, MPI_COMM_WORLD);
                        done = 1;
                    } else {
                        ResultMsg pr = {partials[idx].parent_id, partials[idx].is_left, combined};
                        MPI_Send(&pr, 1, MPI_RESULT_TYPE, partials[idx].parent_rank, TAG_RESULT, MPI_COMM_WORLD);
                    }
                }
            }
        }

        MPI_Iprobe(MPI_ANY_SOURCE, TAG_FINAL_RESULT, MPI_COMM_WORLD, &flag, &status);
        if (flag) { MPI_Recv(&final_result, 1, MPI_LONG_LONG, status.MPI_SOURCE, TAG_FINAL_RESULT, MPI_COMM_WORLD, &status); done = 1; }

        MPI_Iprobe(MPI_ANY_SOURCE, TAG_WORK_REQUEST, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            int req; MPI_Recv(&req, 1, MPI_INT, status.MPI_SOURCE, TAG_WORK_REQUEST, MPI_COMM_WORLD, &status);
            Task resp = {-1, -1, -1, -1, 0};
            if (queue_size(&queue) > 1) resp = queue_pop(&queue);
            MPI_Send(&resp, 1, MPI_TASK_TYPE, req, TAG_WORK_RESPONSE, MPI_COMM_WORLD);
        }

        if (pending_steal >= 0) {
            MPI_Iprobe(pending_steal, TAG_WORK_RESPONSE, MPI_COMM_WORLD, &flag, &status);
            if (flag) {
                Task stolen; MPI_Recv(&stolen, 1, MPI_TASK_TYPE, pending_steal, TAG_WORK_RESPONSE, MPI_COMM_WORLD, &status);
                if (stolen.n >= 0) { queue_push(&queue, stolen); idle_count = 0; }
                pending_steal = -1;
            }
        }

        if (!queue_empty(&queue)) {
            Task t = queue_pop(&queue);
            idle_count = 0;
            if (t.n <= THRESHOLD) {
                long long result = fibonacci_direct(t.n);
                tasks_computed++;
                if (t.parent_id == -1) {
                    final_result = result;
                    for (int p = 0; p < size; p++) if (p != rank) MPI_Send(&result, 1, MPI_LONG_LONG, p, TAG_FINAL_RESULT, MPI_COMM_WORLD);
                    done = 1;
                } else {
                    ResultMsg res = {t.parent_id, t.is_left, result};
                    MPI_Send(&res, 1, MPI_RESULT_TYPE, t.parent_rank, TAG_RESULT, MPI_COMM_WORLD);
                }
            } else {
                int my_id = next_task_id++;
                add_partial(my_id, t.n, t.parent_id, t.parent_rank, t.is_left);
                tasks_split++;
                Task left_task = {t.n - 1, next_task_id++, my_id, rank, 1};
                Task right_task = {t.n - 2, next_task_id++, my_id, rank, 0};
                queue_push(&queue, left_task);
                if (size > 1) {
                    int target = (right_neighbor >= 0) ? right_neighbor : (left_neighbor >= 0) ? left_neighbor : rank;
                    if (target != rank) MPI_Send(&right_task, 1, MPI_TASK_TYPE, target, TAG_TASK, MPI_COMM_WORLD);
                    else queue_push(&queue, right_task);
                } else queue_push(&queue, right_task);
            }
        } else if (pending_steal < 0 && size > 1) {
            int victim = -1;
            if (steal_from_right && right_neighbor >= 0) victim = right_neighbor;
            else if (!steal_from_right && left_neighbor >= 0) victim = left_neighbor;
            else if (right_neighbor >= 0) victim = right_neighbor;
            else if (left_neighbor >= 0) victim = left_neighbor;
            steal_from_right = !steal_from_right;
            if (victim >= 0) { MPI_Send(&rank, 1, MPI_INT, victim, TAG_WORK_REQUEST, MPI_COMM_WORLD); pending_steal = victim; }
            idle_count++;
        } else idle_count++;

        if (size == 1 && idle_count > 100 && queue_empty(&queue) && partial_count == 0) done = 1;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double end_time = MPI_Wtime();

    int total_computed, total_split;
    MPI_Reduce(&tasks_computed, &total_computed, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&tasks_split, &total_split, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    double local_time = end_time - start_time, max_time;
    MPI_Reduce(&local_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("=== P2P CHAIN + Decomposition ===\n");
        printf("fib(%d) = %lld\n", target_fib, final_result);
        printf("Процеси: %d, Задачи: %d, Време: %.3f s\n", size, total_computed, max_time);
    }

    MPI_Type_free(&MPI_TASK_TYPE);
    MPI_Type_free(&MPI_RESULT_TYPE);
    MPI_Finalize();
    return 0;
}
