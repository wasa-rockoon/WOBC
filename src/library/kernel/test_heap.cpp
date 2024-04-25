#include "heap.h"

#ifndef ARDUINO

#include <cassert>
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>


TEST(HeapTest, BasicAssertions) {
  unsigned arena_size = 4096;
  uint8_t arena[arena_size];
  memset(arena, 0, arena_size);

  internal::Heap h(arena, arena_size);

  // h.dump();

  unsigned max_size = 256;

  uint8_t* ptr[max_size / 8];
  unsigned size[max_size / 8];
  unsigned seed[max_size / 8];
  for (unsigned i = 0; i < max_size / 8; i++) ptr[i] = nullptr;

  const unsigned N = 10000;
  unsigned rand = 11;

  for (unsigned n = 0; n < N; n++) {
    uint8_t i = (rand / 1000) % (max_size / 8);
    // printf("N = %d\n", n);
    // printf("RAND %d %d\n", rand, i);
    if (ptr[i] == nullptr) {
      size[i] = rand % (max_size - 1) + 1;

      ptr[i] = (uint8_t *)h.alloc(size[i]);

      if (ptr[i] == nullptr) {
        // printf("NOT ENOUGH SPACE %d\n", size[i]);
      }
      else {
        ASSERT_NE(ptr[i], nullptr);
        ASSERT_EQ(size[i], h.getSize(ptr[i]));
        seed[i] = rand;
        for (int k = 0; k < size[i]; k++)
          ptr[i][k] = (seed[i] + k * 97) % 256;
        // printf("ALLOC %d %d %X\n", i, size[i], ptr[i]);
      }
    }
    else if (rand % 3 == 0) {
      // printf("ADD REF %d %X\n", i, ptr[i]);

      ASSERT_EQ(size[i], h.getSize(ptr[i]));
      for (int k = 0; k < size[i]; k++)
        ASSERT_EQ(ptr[i][k], (seed[i] + k * 97) % 256);

      h.addRef(ptr[i]);
    }
    else {
      // printf("FREE %d %X\n", i, ptr[i]);

      ASSERT_EQ(size[i], h.getSize(ptr[i]));
      for (int k = 0; k < size[i]; k++)
        ASSERT_EQ(ptr[i][k], (seed[i] + k * 97) % 256);

      if (h.releaseRef(ptr[i]) == 0)
        ptr[i] = nullptr;
    }

    // h.dump();

    rand = rand * 1664525 + 1013904223;

    // getchar();
  }

  h.dump();

  printf("complete\n");

}
#endif