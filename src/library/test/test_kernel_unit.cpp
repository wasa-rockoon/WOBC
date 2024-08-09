#include <library/kernel/heap.h>
#include <library/kernel/patricia_tri_tree.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <random>
#include <unity.h>


void test_heap() {
  unsigned arena_size = 4096;
  uint8_t arena[arena_size];
  memset(arena, 0, arena_size);

  kernel::Heap h(arena, arena_size);

  h.dump();

  unsigned max_size = 256;

  uint8_t* ptr[max_size / 8];
  unsigned size[max_size / 8];
  unsigned seed[max_size / 8];
  for (unsigned i = 0; i < max_size / 8; i++) ptr[i] = nullptr;

  const unsigned N = 10000;
  // unsigned rand = 11;
  std::random_device engine;
  unsigned rand = engine();

  for (unsigned n = 0; n < N; n++) {

    // printf("answer: \n");
    // for (int j = 0; j < max_size / 8; j++) {
    //   printf("  %d: %4X %d\n", j, ptr[j] == nullptr ? 0 : ptr[j] - arena, size[j]);
    // }

    uint8_t i = (rand / 1000) % (max_size / 8);
    printf("N = %d\n", n);
    printf("RAND %d %d\n", rand, i);
    if (ptr[i] == nullptr) {
      size[i] = rand % (max_size - 1) + 1;

      ptr[i] = (uint8_t *)h.alloc(size[i]);

      if (ptr[i] == nullptr) {
        printf("NOT ENOUGH SPACE %d\n", size[i]);
      }
      else {
        TEST_ASSERT_NOT_EQUAL(ptr[i], nullptr);
        TEST_ASSERT_EQUAL(size[i], h.getSize(ptr[i]));
        seed[i] = rand;
        for (int k = 0; k < size[i]; k++)
          ptr[i][k] = (seed[i] + k * 97) % 256;
        printf("ALLOC %d %d %X\n", i, size[i], ptr[i] - arena);
      }
    }
    else if (rand % 3 == 0) {
      printf("ADD REF %d %X\n", i, ptr[i] - arena);

      TEST_ASSERT_EQUAL(size[i], h.getSize(ptr[i]));
      for (int k = 0; k < size[i]; k++)
        TEST_ASSERT_EQUAL(ptr[i][k], (seed[i] + k * 97) % 256);

      h.addRef(ptr[i]);
    }
    else {
      printf("RELEASE REF %d %X\n", i, ptr[i] - arena);

      TEST_ASSERT_EQUAL(size[i], h.getSize(ptr[i]));
      for (int k = 0; k < size[i]; k++)
        TEST_ASSERT_EQUAL(ptr[i][k], (seed[i] + k * 97) % 256);

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


class TestTreeNode : public kernel::PatriciaTrieTree<unsigned&>::Node {
public:
  TestTreeNode(uint32_t key = 0, uint32_t mask = 0):
    kernel::PatriciaTrieTree<unsigned&>::Node(key, mask) {}

  void onTraverse(unsigned& arg) override {
    // printf("t %X %X\n", getKey(), getMask());
    arg++;
  }
};

void test_tree() {
  unsigned N = 1000;
  unsigned digit = 0xFFFF + 1;

  std::mt19937 mt;
  std::random_device rnd;
  unsigned seed = rnd();
  // seed = -822521644;
  printf("seed: %d\n", seed);
  mt.seed(seed);

  kernel::PatriciaTrieTree<unsigned&> tree;
  TestTreeNode nodes[N];

  for (int n = 0; n < N; n++) {
    unsigned key = mt() % digit;
    unsigned mask = (mt() % digit) | (mt() % digit);
    nodes[n].setKey(key, mask);
    // printf("#%d: %X %X\n", n, key, mask);
    tree.insert(nodes[n]);
  }

  // tree.print();

  printf("\ncheck:\n");

  for (int key = 0; key < digit; key++) {
    unsigned answer = 0;
    for (int n = 0; n < N; n++) {
      if ((nodes[n].getKey() & nodes[n].getMask())
          == (key & nodes[n].getMask())) {
        // printf("[%X: %X %X]\n", key, nodes[n].getKey(), nodes[n].getMask());
        answer++;
      }
    }

    unsigned traversed = 0;
    tree.traverse(key, traversed);

    // printf("%X: %d %d\n", key, answer, traversed);
    TEST_ASSERT_EQUAL(answer, traversed);
  }
}



int main() {
  UNITY_BEGIN();
  RUN_TEST(test_tree);
  RUN_TEST(test_heap);
  UNITY_END();
  return 0;
}