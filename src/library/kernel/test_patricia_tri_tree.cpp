#include "patricia_tri_tree.h"

#ifndef ARDUINO

#include <cassert>
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <map>
#include <random>


class TestTreeNode : public internal::PatriciaTrieTree<unsigned&>::Node {
public:
  TestTreeNode(uint32_t key = 0, uint32_t mask = 0):
    algorithm::PatriciaTrieTree<unsigned&>::Node(key, mask) {}

  void onTraverse(unsigned& arg) override {
    // printf("t %X %X\n", getKey(), getMask());
    arg++;
  }
};

TEST(TreeTest, BasicAssertions) {

  unsigned N = 1000;
  unsigned digit = 0xFFFF + 1;

  std::mt19937 mt;
  std::random_device rnd;
  unsigned seed = rnd();
  // seed = -822521644;
  printf("seed: %d\n", seed);
  mt.seed(seed);

  algorithm::PatriciaTrieTree<unsigned&> tree;
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
    ASSERT_EQ(answer, traversed);
  }
}

#endif