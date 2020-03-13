#include "threadpool.h"
#include <stdio.h>
#include <errno.h>
#include <semaphore.h>
#include <time.h>

int k, n;

typedef struct row_data {
  pthread_mutex_t lock;
  sem_t ready;
  int sum;
  int count;
  pthread_t writer;
  struct row_data *prev;
} row_data_t;

typedef struct cell_data {
  int v, t;
  row_data_t *row;
} cell_data_t;

void *wait_and_write(void *data) {
  int err;
  row_data_t *row_data = (row_data_t *)data;

  if (row_data->prev != NULL) {
    void *res;

    if ((err = pthread_join(row_data->prev->writer, &res)) != 0) {
      fprintf(stderr, "Błąd pthread_join. kod błędu: %d\n", err); exit(1);
    }
  }

  if (sem_wait(&row_data->ready)) {
    fprintf(stderr, "Błąd sem_wait. Kod błędu: %d\n", errno); exit(1);
  }

  printf("%d\n", row_data->sum);
  return NULL;
}

void calc(void *arg, size_t argsz __attribute__((unused)) ) {
  int err;
  cell_data_t *cell_data = (cell_data_t *)arg;

  struct timespec ts;
  ts.tv_nsec = cell_data->t * 1000000;
  ts.tv_sec = 0;

  nanosleep(&ts, &ts);

  if ((err = pthread_mutex_lock(&cell_data->row->lock)) != 0) {
    fprintf(stderr, "Błąd pthread_mutex_lock. kod błędu: %d\n", err); exit(1);
  }

  cell_data->row->sum += cell_data->v;
  cell_data->row->count++;

  if (cell_data->row->count == n) {
    if (sem_post(&cell_data->row->ready)) {
      fprintf(stderr, "Błąd sem_post. Kod błędu: %d\n", errno); exit(1);
    }
  }

  if ((err = pthread_mutex_unlock(&cell_data->row->lock)) != 0) {
    fprintf(stderr, "Błąd pthread_mutex_unlock. kod błędu: %d\n", err); exit(1);
  }
}

int main() {
  int err;
  pthread_attr_t attr;

  if ((err = pthread_attr_init(&attr)) != 0) {
    fprintf(stderr, "Błąd pthread_attr_init. kod błędu: %d\n", err); exit(1);
  }

  scanf("%d", &k);
  scanf("%d", &n);

  cell_data_t matrix[k][n];
  row_data_t rows[k];

  for (int row = 0; row < k; row++) {
    for (int column = 0; column < n; column++) {
      scanf("%d", &matrix[row][column].v);
      scanf("%d", &matrix[row][column].t);

      matrix[row][column].row = &rows[row];
    }

    if (row > 0) {
      rows[row].prev = &rows[row - 1];
    } else {
      rows[row].prev = NULL;
    }

    rows[row].sum = 0;
    rows[row].count = 0;
    
    if ((err = pthread_mutex_init(&rows[row].lock, NULL)) != 0) {
      fprintf(stderr, "Błąd pthread_mutex_init. kod błędu: %d\n", err); exit(1);
    }

    if (sem_init(&rows[row].ready, 0, 0)) {
      fprintf(stderr, "Błąd sem_init. Kod błędu: %d\n", errno); exit(1);
    }

    if ((err = pthread_create(&rows[row].writer, &attr, &wait_and_write, &rows[row])) != 0) {
      fprintf(stderr, "Błąd pthread_create. kod błędu: %d\n", err); exit(1);
    }
  }

  thread_pool_t pool;

  if (thread_pool_init(&pool, 4) != 0) {
    fprintf(stderr, "Błąd thread_pool_init.\n"); exit(1);
  }

  for (int row = 0; row < k; row++) {
    for (int column = 0; column < n; column++) {
      runnable_t runnable;
      runnable.function = &calc;
      runnable.arg = &matrix[row][column];
      runnable.argsz = sizeof(matrix[row][column]);
      
      if ((err = defer(&pool, runnable)) != 0) {
        fprintf(stderr, "Błąd defer.\n"); exit(1);
      }
    }
  }

  void *res;
  if ((err = pthread_join(rows[k - 1].writer, &res)) != 0) {
    fprintf(stderr, "Błąd pthread_join. kod błędu: %d\n", err); exit(1);
  }
  
  thread_pool_destroy(&pool);
  
  for (int row = 0; row < k; row++) {
    if ((err = pthread_mutex_destroy(&rows[row].lock)) != 0) {
      fprintf(stderr, "Błąd pthread_mutex_destroy. kod błędu: %d\n", err); exit(1);
    }

    if (sem_destroy(&rows[row].ready)) {
      fprintf(stderr, "Błąd sem_destroy. Kod błędu: %d\n", errno); exit(1);
    }
  }

  if ((err = pthread_attr_destroy(&attr)) != 0) {
    fprintf(stderr, "Błąd pthread_attr_destroy. kod błędu: %d\n", err); exit(1);
  }

  return 0;
}