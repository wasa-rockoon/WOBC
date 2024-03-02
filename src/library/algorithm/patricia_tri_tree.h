// Patricia Trie Tree

#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <cassert>
#include <stdint.h>
#include <stdio.h>
#endif


namespace algorithm {


template<typename T>
class PatriciaTrieTree {
public:

  using key_t = uint32_t;

  class Node {
  public:
    Node(key_t key, key_t mask = ~(key_t)0)
      : key_(key), mask_(mask), label_(~(key_t)0),
        bit0_(nullptr), bit1_(nullptr), dontcare_(nullptr) {}

    inline Node& setKey(key_t key, key_t mask = ~(key_t)0) {
      key_ = key;
      mask_ = mask;
      return *this;
    }

    inline key_t getKey() const { return key_; }
    inline key_t getMask() const { return mask_; }

  protected:
    virtual void onTraverse(T arg){};

  private:
    key_t key_;
    key_t mask_;
    key_t label_;
    Node *bit0_;
    Node *bit1_;
    Node *dontcare_;

    Node* insert(Node* node) {
      key_t diff = (key_ ^ node->key_) | (mask_ ^ node->mask_);
      key_t new_label = (-diff & diff) - 1;

      Node* parent = this;
      Node* child = node;
      if (new_label < label_) {
        node->label_ = new_label;
        parent = node;
        child = this;
      }

      key_t selection = (parent->label_ << 1) & ~parent->label_;
      if (selection == 0) selection = 1;

      if (selection & ~child->mask_) { // dont care
        if (parent->dontcare_ == nullptr) parent->dontcare_ = child;
        else parent->dontcare_ = parent->dontcare_->insert(child);
      }
      else if (selection & child->key_) { // bit 1
        if (parent->bit1_ == nullptr) parent->bit1_ = child;
        else parent->bit1_ = parent->bit1_->insert(child);
      }
      else { // bit 0
        if (parent->bit0_ == nullptr) parent->bit0_ = child;
        else parent->bit0_ = parent->bit0_->insert(child);
      }

      return parent;
    }

    void traverse(key_t key, T arg) {
      if ((label_ & mask_ & key) != (label_ & mask_ & key_)) return;
      if (match(key)) onTraverse(arg);
      key_t selection = (label_ << 1) & ~label_;
      if (selection == 0) selection = 1;

      if (selection & key) { // bit 1
        if (bit1_ != nullptr) bit1_->traverse(key, arg);
      }
      else { // bit 0
        if (bit0_ != nullptr) bit0_->traverse(key, arg);
      }
      if (dontcare_ != nullptr) dontcare_->traverse(key, arg);
    }
    inline bool match(key_t key) {
      return (key_ & mask_) == (key & mask_);
    }

    void print(int indent) {
      printf("%*s", indent, "");
      printf("|%8X %8X %8X\n", key_, mask_, label_);
      printf("%*s", indent, "");
      printf("1: \n");
      if (bit1_ != nullptr) bit1_->print(indent + 2);
      printf("%*s", indent, "");
      printf("0: \n");
      if (bit0_ != nullptr) bit0_->print(indent + 2);
      printf("%*s", indent, "");
      printf("_: \n");
      if (dontcare_ != nullptr) dontcare_->print(indent + 2);
    }

    friend PatriciaTrieTree<T>;
  };

  PatriciaTrieTree() : root_(nullptr) {}

  inline void traverse(key_t key, T arg) {
    return root_->traverse(key, arg);
  };


  inline Node& insert(Node& node) {
    if (root_ == nullptr) {
      root_ = &node;
    }
    else {
      root_ = root_->insert(&node);
    }
    return node;
  }

  void print() {
    if (root_ != nullptr) root_->print(0);
  }

private:

  Node* root_;
};

} // namespace algorithm


