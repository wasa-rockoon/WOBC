#include <library/kernel/heap.h>
#include <library/kernel/patricia_tri_tree.h>
#include <components/IGN/IGNSequence.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <random>
#include <unity.h>

void setUp() {}
void tearDown() {}

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

void test_ign_sequence_transitions() {
  component::IGNSequence sequence;

  auto state = sequence.update(0);
  TEST_ASSERT_EQUAL((int)component::IGNSequence::Phase::Disarmed,
                    (int)state.phase);
  TEST_ASSERT_FALSE(state.high);
  TEST_ASSERT_FALSE(state.low);

  TEST_ASSERT_TRUE(sequence.start(0));
  state = sequence.update(0);
  TEST_ASSERT_EQUAL((int)component::IGNSequence::Phase::Startup,
                    (int)state.phase);
  TEST_ASSERT_TRUE(state.high);
  TEST_ASSERT_FALSE(state.low);
  TEST_ASSERT_EQUAL_UINT32(36000, state.remaining_ms);

  state = sequence.update(999);
  TEST_ASSERT_EQUAL((int)component::IGNSequence::Phase::Startup,
                    (int)state.phase);
  state = sequence.update(1000);
  TEST_ASSERT_EQUAL((int)component::IGNSequence::Phase::Countdown,
                    (int)state.phase);
  TEST_ASSERT_TRUE(state.phase_changed);

  state = sequence.update(30999);
  TEST_ASSERT_EQUAL((int)component::IGNSequence::Phase::Countdown,
                    (int)state.phase);
  state = sequence.update(31000);
  TEST_ASSERT_EQUAL((int)component::IGNSequence::Phase::Final,
                    (int)state.phase);
  TEST_ASSERT_TRUE(state.high);
  TEST_ASSERT_FALSE(state.low);

  state = sequence.update(35999);
  TEST_ASSERT_EQUAL((int)component::IGNSequence::Phase::Final,
                    (int)state.phase);
  state = sequence.update(36000);
  TEST_ASSERT_EQUAL((int)component::IGNSequence::Phase::Ignition,
                    (int)state.phase);
  TEST_ASSERT_TRUE(state.high);
  TEST_ASSERT_TRUE(state.low);

  state = sequence.update(38999);
  TEST_ASSERT_EQUAL((int)component::IGNSequence::Phase::Ignition,
                    (int)state.phase);
  state = sequence.update(39000);
  TEST_ASSERT_EQUAL((int)component::IGNSequence::Phase::Done,
                    (int)state.phase);
  TEST_ASSERT_FALSE(state.high);
  TEST_ASSERT_FALSE(state.low);
}

void test_ign_sequence_never_energizes_after_expiry() {
  component::IGNSequence sequence;
  TEST_ASSERT_TRUE(sequence.start(0));
  sequence.update(1000);
  sequence.update(31000);
  auto state = sequence.update(36000);
  TEST_ASSERT_EQUAL((int)component::IGNSequence::Phase::Ignition,
                    (int)state.phase);

  // If the next update is late, it must go directly to a safe Done output.
  state = sequence.update(45000);
  TEST_ASSERT_EQUAL((int)component::IGNSequence::Phase::Done,
                    (int)state.phase);
  TEST_ASSERT_FALSE(state.high);
  TEST_ASSERT_FALSE(state.low);
}

void test_ign_sequence_abort_fault_and_single_start() {
  component::IGNSequence sequence;
  TEST_ASSERT_TRUE(sequence.start(10));
  sequence.abort(20);
  auto state = sequence.update(20);
  TEST_ASSERT_EQUAL((int)component::IGNSequence::Phase::Disarmed,
                    (int)state.phase);
  TEST_ASSERT_FALSE(state.high);
  TEST_ASSERT_FALSE(state.low);
  TEST_ASSERT_FALSE(sequence.start(21));

  component::IGNSequence faulted;
  faulted.fault(30);
  state = faulted.update(30);
  TEST_ASSERT_EQUAL((int)component::IGNSequence::Phase::Fault,
                    (int)state.phase);
  TEST_ASSERT_FALSE(state.high);
  TEST_ASSERT_FALSE(state.low);
  TEST_ASSERT_FALSE(faulted.start(31));
}

void test_ign_sequence_millis_wraparound() {
  component::IGNSequence sequence;
  const uint32_t start = UINT32_MAX - 499;
  TEST_ASSERT_TRUE(sequence.start(start));

  const auto state = sequence.update(500);
  TEST_ASSERT_EQUAL((int)component::IGNSequence::Phase::Countdown,
                    (int)state.phase);
  TEST_ASSERT_FALSE(state.low);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_tree);
  RUN_TEST(test_heap);
  RUN_TEST(test_ign_sequence_transitions);
  RUN_TEST(test_ign_sequence_never_energizes_after_expiry);
  RUN_TEST(test_ign_sequence_abort_fault_and_single_start);
  RUN_TEST(test_ign_sequence_millis_wraparound);
  UNITY_END();
  return 0;
}
