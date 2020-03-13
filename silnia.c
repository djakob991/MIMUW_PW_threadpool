#include "future.h"
#include <stdio.h>

int n;

typedef struct partial_product {
  int num;
  int val;
} partial_product_t;

void *next_product(void *arg, size_t argsz __attribute__((unused)), size_t *res_sz) {
  partial_product_t *product = (partial_product_t *)arg;
  partial_product_t *res = (partial_product_t *)malloc(sizeof(partial_product_t));
  
  if (res == NULL) {
    fprintf(stderr, "Błąd alokacji w next_product.\n"); exit(1);
  }

  if (product == NULL) {
    res->num = 0;
    res->val = 1;
  
  } else {
    res->num = product->num + 1;
    res->val = product->val * res->num;
  }
  
  *res_sz = sizeof(*res);
  return res;
}

int main () {
  scanf("%d", &n);

  thread_pool_t pool;

  if (thread_pool_init(&pool, 3) != 0) {
    fprintf(stderr, "Błąd thread_pool_init.\n"); exit(1);
  }

  future_t fut[n + 1];

  callable_t init;
  init.function = &next_product;
  init.arg = NULL;
  init.argsz = 0;
  
  if (async(&pool, &fut[0], init) != 0) {
    fprintf(stderr, "Błąd async.\n"); exit(1);
  }

  for (int i = 1; i <= n; i++) {
    if (map(&pool, &fut[i], &fut[i - 1], &next_product) != 0) {
      fprintf(stderr, "Błąd map.\n"); exit(1);
    }
  }

  partial_product_t *final_product = await(&fut[n]);
  printf("%d\n", final_product->val);

  thread_pool_destroy(&pool);

  for (int i = 0; i <= n; i++) {
    free(fut[i].val);
  }

  return 0;
}