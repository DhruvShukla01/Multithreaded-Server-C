/*
 * Thread-pool TCP echo server.
 *
 * Architecture (vs. multiServ.c's thread-per-connection):
 *   - Fixed pool of worker threads created at startup (no per-client
 *     pthread_create cost, no unbounded thread growth under load).
 *   - Accepted sockets are handed to workers through a BOUNDED REQUEST
 *     QUEUE (ring buffer + mutex + two condition variables). When the queue
 *     is full the acceptor blocks, applying backpressure instead of
 *     spawning without limit.
 *   - Shared server statistics are protected by a pthread_rwlock_t:
 *     per-message updates take the write lock; any client can send the
 *     literal message "STATS" to read a consistent snapshot (read lock,
 *     concurrent readers don't serialize).
 *
 * Usage: ./server_pool [port] [workers] [queue_capacity]
 *   defaults: 8081, 128, 256
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define BUFFER_SIZE 1024
#define LISTEN_BACKLOG 128

/* ── Bounded request queue ─────────────────────────────────────────── */

typedef struct {
    int *items;
    int capacity;
    int head, tail, count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} request_queue_t;

static void queue_init(request_queue_t *q, int capacity) {
    q->items = malloc(sizeof(int) * capacity);
    q->capacity = capacity;
    q->head = q->tail = q->count = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

/* Blocks while the queue is full (backpressure on the acceptor). */
static void queue_push(request_queue_t *q, int fd) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == q->capacity)
        pthread_cond_wait(&q->not_full, &q->mutex);
    q->items[q->tail] = fd;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

/* Blocks while the queue is empty. */
static int queue_pop(request_queue_t *q) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == 0)
        pthread_cond_wait(&q->not_empty, &q->mutex);
    int fd = q->items[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return fd;
}

/* ── Shared stats behind a read-write lock ─────────────────────────── */

typedef struct {
    long total_connections;
    long total_messages;
    long total_bytes;
    pthread_rwlock_t lock;
} server_stats_t;

static server_stats_t stats = { 0, 0, 0, PTHREAD_RWLOCK_INITIALIZER };

static void stats_record_connection(void) {
    pthread_rwlock_wrlock(&stats.lock);
    stats.total_connections++;
    pthread_rwlock_unlock(&stats.lock);
}

static void stats_record_message(long bytes) {
    pthread_rwlock_wrlock(&stats.lock);
    stats.total_messages++;
    stats.total_bytes += bytes;
    pthread_rwlock_unlock(&stats.lock);
}

static int stats_snapshot(char *out, size_t out_len) {
    pthread_rwlock_rdlock(&stats.lock);
    int n = snprintf(out, out_len,
                     "STATS connections=%ld messages=%ld bytes=%ld\n",
                     stats.total_connections, stats.total_messages,
                     stats.total_bytes);
    pthread_rwlock_unlock(&stats.lock);
    return n;
}

/* ── Client handling (same echo protocol as multiServ.c) ───────────── */

static void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t read_size;

    stats_record_connection();

    const char *welcome = "Welcome to the C multithreaded server!\n";
    write(client_socket, welcome, strlen(welcome));

    while ((read_size = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        /* "STATS" query path: exercises the rwlock read side. */
        if (read_size >= 5 && strncmp(buffer, "STATS", 5) == 0) {
            char reply[128];
            int n = stats_snapshot(reply, sizeof(reply));
            write(client_socket, reply, n);
            continue;
        }
        write(client_socket, buffer, read_size); /* exact-byte echo */
        stats_record_message(read_size);
    }

    close(client_socket);
}

static request_queue_t queue;

static void *worker_main(void *arg) {
    (void)arg;
    for (;;) {
        int fd = queue_pop(&queue);
        handle_client(fd);
    }
    return NULL;
}

/* ── Main: listen + accept loop as producer ────────────────────────── */

int main(int argc, char *argv[]) {
    int port = argc > 1 ? atoi(argv[1]) : 8081;
    int workers = argc > 2 ? atoi(argv[2]) : 128;
    int capacity = argc > 3 ? atoi(argv[3]) : 256;

    /* A write() to a disconnected client must not kill the server. */
    signal(SIGPIPE, SIG_IGN);

    queue_init(&queue, capacity);

    pthread_t tid;
    for (int i = 0; i < workers; i++) {
        if (pthread_create(&tid, NULL, worker_main, NULL) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
        pthread_detach(tid);
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    int one = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if (listen(server_socket, LISTEN_BACKLOG) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Thread-pool server: port=%d workers=%d queue=%d\n",
           port, workers, capacity);

    for (;;) {
        int client = accept(server_socket, NULL, NULL);
        if (client < 0) {
            perror("accept");
            continue;
        }
        queue_push(&queue, client);
    }
}
