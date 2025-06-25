#include <klib.h>
#include "cache-utils.h"

uint8_t array2[8 * L1_BLOCK_SZ_BYTES];

static inline void flushCacheLine(void* addr) {
  asm volatile (
          "cbo.flush 0(%0)"
          :
          : "r"(addr)
          : "memory");
}

static inline void fence() {
  asm volatile ("fence rw, rw" ::: "memory");
}


int main(void) {
  uint64_t start=0, end=0, diff=0;
  uint8_t volatile dummy = 0;
  printf("data cache test \n");

  for(int i=0; i<4; i++) {
    //flushCache((uint64_t)array2, sizeof(array2)); //flushCache works on the entire array
    for (uint64_t i = 0; i < 8; ++i) {
      flushCacheLine(&array2[i * L1_BLOCK_SZ_BYTES]);

      fence();
      asm volatile("csrr %0, mcycle" : "=r"(start));

      dummy &= array2[i * L1_BLOCK_SZ_BYTES];

      fence();
      asm volatile("csrr %0, mcycle" : "=r"(end));

      diff = end - start;
      printf("diff read from ram[%d]: %d\n", i, diff);

      fence();
      asm volatile("csrr %0, mcycle" : "=r"(start));

      dummy &= array2[i * L1_BLOCK_SZ_BYTES];

      fence();
      asm volatile("csrr %0, mcycle" : "=r"(end));

      diff = end - start;
      printf("diff from data cache[%d]:  %d\n\n", i, diff);
    }
  }
  return 0;
}
