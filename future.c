#include "future.h"
#include <errno.h>

void executer(void *arg, size_t argsz __attribute__((unused))) {
  future_t *future = (future_t *)arg;

  void *(*function)(void *, size_t, size_t *) = future->callable.function;
  void *fun_arg = future->callable.arg;
  size_t fun_argsz = future->callable.argsz;

  future->val = (*function)(fun_arg, fun_argsz, &future->size);
  
  if (sem_post(&future->ready)) {
    fprintf(stderr, "Błąd sem_post. Kod błędu: %d\n", errno); exit(1);
  }
}

int async(thread_pool_t *pool, future_t *future, callable_t callable) {
  int err;

  future->callable = callable;

  if (sem_init(&future->ready, 0, 0)) {
    fprintf(stderr, "Błąd sem_init. Kod błędu: %d\n", errno); exit(1);
  }

  runnable_t runnable;
  runnable.function = &executer;
  runnable.arg = future;
  runnable.argsz = sizeof(*future);

  if ((err = defer(pool, runnable)) != 0) {
    return err;
  }

  return 0;
}

typedef struct map_container {
  future_t *future, *from;
  void *(*function)(void *, size_t, size_t *);
} map_container_t;

void map_executer(void *arg, size_t argsz __attribute__((unused))) {
  map_container_t *map_container = (map_container_t *)arg;
  
  future_t *future = map_container->future;
  future_t *from = map_container->from;
  void *(*new_function)(void *, size_t, size_t *) = map_container->function;
  free(map_container);

  if (sem_wait(&from->ready)) {
    fprintf(stderr, "Błąd sem_wait. Kod błędu: %d\n", errno); exit(1);
  }

  if (sem_destroy(&from->ready)) {
    fprintf(stderr, "Błąd sem_destroy. Kod błędu: %d\n", errno); exit(1);
  }

  future->val = (*new_function)(from->val, from->size, &future->size);

  if (sem_post(&future->ready)) {
    fprintf(stderr, "Błąd sem_post. Kod błędu: %d\n", errno); exit(1);
  }
}

int map(thread_pool_t *pool, future_t *future, future_t *from, void *(*function)(void *, size_t, size_t *)) {
  int err;

  if (sem_init(&future->ready, 0, 0)) {
    fprintf(stderr, "Błąd sem_init. Kod błędu: %d\n", errno); exit(1);
  }

  map_container_t *new_map_containter = (map_container_t * )malloc(sizeof(map_container_t));

  if (new_map_containter == NULL) {
    if (sem_destroy(&future->ready)) {
      fprintf(stderr, "Błąd sem_destroy. Kod błędu: %d\n", errno); exit(1);
    }

    return 1;
  }

  new_map_containter->future = future;
  new_map_containter->from = from;
  new_map_containter->function = function;

  runnable_t runnable;
  runnable.function = &map_executer;
  runnable.arg = new_map_containter;
  runnable.argsz = sizeof(*new_map_containter);

  if ((err = defer(pool, runnable)) != 0){
    if (sem_destroy(&future->ready)) {
     fprintf(stderr, "Błąd sem_destroy. Kod błędu: %d\n", errno); exit(1);
    }

    free(new_map_containter);
    return err;
  }

  return 0;
}

void *await(future_t *future) {
  if (sem_wait(&future->ready)) {
    fprintf(stderr, "Błąd sem_wait. Kod błędu: %d\n", errno); exit(1);
  }

  if (sem_destroy(&future->ready)) {
    fprintf(stderr, "Błąd sem_destroy. Kod błędu: %d\n", errno); exit(1);
  }

  return future->val;
}
