#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <string>
#include <stack>
#include <iostream>
#include <list>

#pragma pack(push, 1) // exact fit - no padding

class Node {
 public:
  enum class Type : char {
    Constant,
    Add,
  };

  size_t realSize();

  const Type type;
  Node(const Type type) : type(type) {}

  virtual void print(std::ostream& os) const = 0;
  friend std::ostream& operator<<(std::ostream& os, const Node& n) {
    n.print(os);
    return os;
  }

  virtual ~Node() {}
};

class Constant : public Node {
 public:
  const int value;
  Constant(int value) : Node(Type::Constant), value(value) {}

  void print(std::ostream& os) const override {
    os << value;
  }
};


class Add : public Node {
  Node* l_;
  Node* r_;

 public:
  Add(Node* l, Node* r) : Node(Type::Add), l_(l), r_(r) {}

  Node* l() const {
    return l_;
  }

  Node* r() const {
    return r_;
  }

  void print(std::ostream& os) const override {
    os << *l() << " + " << *r();
  }
};

#pragma pack(pop) // exact fit - no padding

class NodeList : public std::list<Node*> {
 public:
  ~NodeList() {
    for (auto n : *this)
      delete n;
  }

  std::list<Node*>::iterator at(size_t pos) {
    auto it = this->begin();
    std::advance(it, pos);
    return it;
  }

  Node* push_back(Node* n) {
    std::list<Node*>::push_back(n);
    return n;
  }

  Node* insert(std::list<Node*>::iterator it, Node* n) {
    std::list<Node*>::insert(it, n);
    return n;
  }
};

