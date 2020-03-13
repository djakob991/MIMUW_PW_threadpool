#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>

typedef struct runnable {
  void (*function)(void *, size_t);
  void *arg;
  size_t argsz;
} runnable_t;

typedef struct queue_node {
  runnable_t val;
  struct queue_node *next;
} queue_node_t;

typedef struct queue {
  queue_node_t *first, *last;
  size_t size;
} queue_t;

typedef struct thread_pool {
  size_t num_threads;
  pthread_attr_t attr;
  pthread_t *threads; // tablica deskryptorów
  pthread_mutex_t lock;
  pthread_cond_t for_tasks;
  queue_t *task_queue;
  bool destroyed;  
} thread_pool_t;

int thread_pool_init(thread_pool_t *pool, size_t pool_size);

void thread_pool_destroy(thread_pool_t *pool);

int defer(thread_pool_t *pool, runnable_t runnable);

#endif
