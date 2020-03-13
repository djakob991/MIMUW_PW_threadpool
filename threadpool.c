#include "threadpool.h"

bool create_queue_node(queue_node_t **dest, runnable_t val) {
  queue_node_t *new_queue_node = (queue_node_t *)malloc(sizeof(queue_node_t));
  
  if (new_queue_node == NULL) {
    return false;
  }
  
  new_queue_node->val = val;
  new_queue_node->next = NULL;

  *dest = new_queue_node;
  return true;
}

bool create_queue(queue_t **dest){
  queue_t *new_queue = (queue_t *)malloc(sizeof(queue_t));
  
  if (new_queue == NULL) {
    return false;
  }
  
  new_queue->first = NULL;
  new_queue->last = NULL;
  new_queue->size = 0;
  
  *dest = new_queue;
  return true;
}

runnable_t get_first(queue_t *queue) {
  runnable_t result = queue->first->val;
  queue_node_t *tmp = queue->first;
  queue->first = queue->first->next;
  free(tmp);
  queue->size--;

  return result;
} 

bool append(queue_t *queue, runnable_t runnable) {
  queue_node_t *new_node;

  if (!create_queue_node(&new_node, runnable)) {
    return false;
  }

  if (queue->first == NULL) {
    queue->first = new_node;
  
  } else {
    queue->last->next = new_node;
  }

  queue->last = new_node;
  queue->size++;

  return true;
}

void *worker(void *data) {
  int err;
  thread_pool_t *pool = (thread_pool_t *)data;
  runnable_t task;

  while (true) {
    if ((err = pthread_mutex_lock(&pool->lock)) != 0) {
      fprintf(stderr, "Błąd pthread_mutex_lock. kod błędu: %d\n", err); exit(1);
    }

    while (pool->task_queue->size == 0) {
      if (pool->destroyed) {
        if ((err = pthread_mutex_unlock(&pool->lock)) != 0) {
           fprintf(stderr, "Błąd pthread_mutex_unlock. kod błędu: %d\n", err); exit(1); 
        }

        pthread_exit(NULL);
      }
      
      if ((err = pthread_cond_wait(&pool->for_tasks, &pool->lock)) != 0) {
        fprintf(stderr, "Błąd pthread_cond_wait. kod błędu: %d\n", err); exit(1);
      }
    }

    task = get_first(pool->task_queue);

    if ((err = pthread_mutex_unlock(&pool->lock)) != 0) {
      fprintf(stderr, "Błąd pthread_mutex_unlock. kod błędu: %d\n", err); exit(1); 
    }

    (task.function)(task.arg, task.argsz);
  }
}

int thread_pool_init(thread_pool_t *pool, size_t num_threads) {
  int err;

  if ((err = pthread_mutex_init(&pool->lock, NULL)) != 0) {
    fprintf(stderr, "Błąd pthread_mutex_init. kod błędu: %d\n", err); exit(1); 
  }

  if ((err = pthread_mutex_lock(&pool->lock)) != 0) {
    fprintf(stderr, "Błąd pthread_mutex_lock. kod błędu: %d\n", err); exit(1); 
  }

  pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);

  if (pool->threads == NULL) {
    if ((err = pthread_mutex_destroy(&pool->lock)) != 0) {
      fprintf(stderr, "Błąd pthread_mutex_destroy. kod błędu: %d\n", err); exit(1); 
    }

    return 1;
  }

  if (!create_queue(&pool->task_queue)) {
    if ((err = pthread_mutex_destroy(&pool->lock)) != 0) {
      fprintf(stderr, "Błąd pthread_mutex_destroy. kod błędu: %d\n", err); exit(1); 
    }

    return 1;
  }

  if ((err = pthread_attr_init(&pool->attr)) != 0) {
    fprintf(stderr, "Błąd pthread_attr_init. kod błędu: %d\n", err); exit(1); 
  }

  for (size_t i = 0; i < num_threads; i++) {
    if ((err = pthread_create(&pool->threads[i], &pool->attr, &worker, pool)) != 0) {
      fprintf(stderr, "Błąd pthread_create. kod błędu: %d\n", err); exit(1); 
    }
  }

  if ((err = pthread_cond_init(&pool->for_tasks, NULL)) != 0) {
    fprintf(stderr, "Błąd pthread_cond_init. kod błędu: %d\n", err); exit(1); 
  }
  
  pool->destroyed = false;
  pool->num_threads = num_threads;

  if ((err = pthread_mutex_unlock(&pool->lock)) != 0) {
    fprintf(stderr, "Błąd pthread_mutex_unlock. kod błędu: %d\n", err); exit(1); 
  }

  return 0;
}

void thread_pool_destroy(struct thread_pool *pool) {
  int err;

  if ((err = pthread_mutex_lock(&pool->lock)) != 0) {
    fprintf(stderr, "Błąd pthread_mutex_lock. kod błędu: %d\n", err); exit(1); 
  }

  pool->destroyed = true;
  pthread_cond_broadcast(&pool->for_tasks);

  if ((err = pthread_mutex_unlock(&pool->lock)) != 0) {
    fprintf(stderr, "Błąd pthread_mutex_unlock. kod błędu: %d\n", err); exit(1); 
  }

  for (size_t i = 0; i < pool->num_threads; i++) {
    void *res;

    if ((err = pthread_join(pool->threads[i], &res)) != 0) {
      fprintf(stderr, "Błąd pthread_join. kod błędu: %d\n", err); exit(1); 
    }
  }

  free(pool->threads);

  if ((err = pthread_mutex_destroy(&pool->lock)) != 0) {
    fprintf(stderr, "Błąd pthread_mutex_destroy. kod błędu: %d\n", err); exit(1); 
  }

  if ((err = pthread_attr_destroy(&pool->attr)) != 0) {
    fprintf(stderr, "Błąd pthread_attr_destroy. kod błędu: %d\n", err); exit(1); 
  }

  if ((err = pthread_cond_destroy(&pool->for_tasks)) != 0) {
    fprintf(stderr, "Błąd pthread_cond_destroy. kod błędu: %d\n", err); exit(1); 
  }

  free(pool->task_queue);
}

int defer(struct thread_pool *pool, runnable_t runnable) {
  int err;
  
  if (pool->destroyed) {
    return 1;
  }

  if ((err = pthread_mutex_lock(&pool->lock)) != 0) {
    fprintf(stderr, "Błąd pthread_mutex_lock. kod błędu: %d\n", err); exit(1); 
  }

  if (pool->destroyed) {
    if ((err = pthread_mutex_unlock(&pool->lock)) != 0) {
      fprintf(stderr, "Błąd pthread_mutex_unlock. kod błędu: %d\n", err); exit(1); 
    }

    return 1; // error
  }

  if (!append(pool->task_queue, runnable)) {
    if ((err = pthread_mutex_unlock(&pool->lock)) != 0) {
      fprintf(stderr, "Błąd pthread_mutex_unlock. kod błędu: %d\n", err); exit(1); 
    }
    
    return 2; // error
  }

  if ((err = pthread_cond_signal(&pool->for_tasks)) != 0) {
    fprintf(stderr, "Błąd pthread_cond_signal. kod błędu: %d\n", err); exit(1); 
  }

  if ((err = pthread_mutex_unlock(&pool->lock)) != 0) {
    fprintf(stderr, "Błąd pthread_mutex_unlock. kod błędu: %d\n", err); exit(1); 
  }

  return 0;
}
