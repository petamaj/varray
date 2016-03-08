#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <string>
#include <stack>
#include <iostream>

#pragma pack(push, 1) // exact fit - no padding

class Node {
 public:
  enum class Type : char {
    Constant,
    Add,
  };

  inline size_t realSize() const;

  const Type type;
  Node(const Type type) : type(type) {}

  virtual void print(std::ostream& os) const = 0;
  friend std::ostream& operator<<(std::ostream& os, const Node& n) {
    n.print(os);
    return os;
  }

  virtual void adjustOffset(Node * n) {
    *(Node**)n = this;
  }
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
  intptr_t offset_l;
  intptr_t offset_r;

 public:
  Add(Node* l, Node* r) : Node(Type::Add),
                          offset_l((intptr_t)l - (intptr_t)this),
                          offset_r((intptr_t)r - (intptr_t)this) {}

  Node* l() const {
    return (Node*)((intptr_t)this + offset_l);
  }

  Node* r() const {
    return (Node*)((intptr_t)this + offset_r);
  }

  void print(std::ostream& os) const override {
    os << *l() << " + " << *r();
  }

  void adjustOffset(Node* old_) override {
    Add* old = static_cast<Add*>(old_);
    Node * up_r = *(Node**)old->r();
    Node * up_l = *(Node**)old->l();
    offset_l = (intptr_t)up_l - (intptr_t)this;
    offset_r = (intptr_t)up_r - (intptr_t)this;
    Node::adjustOffset(old_);
  }
};

size_t Node::realSize() const {
  switch(type) {
    case Type::Constant: return sizeof(Constant);
    case Type::Add: return sizeof(Add);
  }
}

#pragma pack(pop) // exact fit - no padding

class NodeList {
  static constexpr size_t size = 10*1024*1024;
  static constexpr size_t gapSize = sizeof(NodeList*);

  uintptr_t buf;
  uintptr_t pos;

 public:
  NodeList() {
    buf = (uintptr_t)malloc(size);
    *(NodeList**)buf = nullptr;
    pos = buf + gapSize;
  }

  ~NodeList() {
    uintptr_t finger = buf;
    while (true) {
      NodeList** gap = reinterpret_cast<NodeList**>(finger);
      if (*gap) delete *gap;
      finger += gapSize;
      if (finger >= pos)
        break;
      auto n = (Node*)finger;
      finger += n->realSize();
    }
    free((void*)buf);
  }

  template<typename Node>
  Node* insert() {
    return new(prepareInsert(sizeof(Node))) Node();
  }

  template<typename Node, typename Arg1>
  Node* insert(Arg1 arg1) {
    return new(prepareInsert(sizeof(Node))) Node(arg1);
  }

  template<typename Node, typename Arg1, typename Arg2>
  Node* insert(Arg1 arg1, Arg2 arg2) {
    return new(prepareInsert(sizeof(Node))) Node(arg1, arg2);
  }

  void* prepareInsert(size_t s) {
    void* res = (void*)pos;
    pos += s;
    *(NodeList**)pos = nullptr;
    pos += gapSize;
    assert((uintptr_t)pos < (uintptr_t)buf + size);
    return res;
  }

  inline Node* insert(Node* n) {
    size_t s = n->realSize();
    memcpy((void*)pos, (void*)n, s);
    auto res = (Node*)pos;
    pos += s;
    *(NodeList**)pos = nullptr;
    pos += gapSize;
    return res;
  }

  NodeList* flatten() {
    auto flat = new NodeList();

    constexpr int bufSize = 1024;

    Node* bulkFixup[bufSize];
    int fixupBuffer = 0;

    // Bulk copy, to avoid doing insert(Node*) for every element
    auto fixup = [&fixupBuffer, &bulkFixup]
                 (uintptr_t pos) -> uintptr_t {
      Node* first = bulkFixup[0];
      Node* last = bulkFixup[fixupBuffer-1];
      size_t s = (uintptr_t)last - (uintptr_t)first + last->realSize();
      memcpy((void*)pos, (void*)first, s);

      uintptr_t finger;

      finger = pos;
      for (int i = 0; i < fixupBuffer && i < bufSize; ++i) {
        Node* old = bulkFixup[i];
        Node* copy = (Node*)finger;
        size_t s = copy->realSize();
        copy->adjustOffset(old);
        *((NodeList**)(finger - gapSize)) = nullptr;
        finger += s + gapSize;
      }

      pos = finger;

      *((NodeList**)(pos - gapSize)) = nullptr;
      fixupBuffer = 0;

      return pos;
    };

    int depth = 0;
    for (auto i = begin(); i.isEnd(); ++i) {
      if (fixupBuffer == bufSize || i.worklist.size() != depth) {
        flat->pos = fixup(flat->pos);
        depth = i.worklist.size();
      }
      bulkFixup[fixupBuffer++] = *i;
    }
    flat->pos = fixup(flat->pos);

    return flat;
  }

  class iterator {
    uintptr_t pos;
    uintptr_t end;
    std::stack<uintptr_t> worklist;

    void popWorklist() {
        end = worklist.top();
        worklist.pop();
        pos = worklist.top();
        worklist.pop();
    }

    inline void findStart() {
      while (true) {
        NodeList** gapPos =
            reinterpret_cast<NodeList**>(pos - gapSize);
        NodeList* gap = *gapPos;
        if (!gap) {
          break;
        }
        worklist.push(pos);
        worklist.push(end);
        pos = gap->buf + gapSize;
        end = gap->pos;
      }
    }

   public:
    iterator(uintptr_t start, uintptr_t end) : pos(start), end(end) {
      if (start != -1)
        findStart();
    }

    bool operator != (iterator& other) {
      assert(other.pos == -1 && other.end == -1);
      return isEnd();
    }

    bool isEnd() {
      while (pos == end && !worklist.empty()) {
        popWorklist();
      }
      return !worklist.empty() || pos != end;
    }

    void operator ++ () {
      if (pos == end) {
        while (pos == end) {
          assert(!worklist.empty());
          popWorklist();
        }
        return;
      }
      Node* n = this->operator*();
      pos += n->realSize() + gapSize;
      if (pos == end) {
        while (pos == end && !worklist.empty()) {
          popWorklist();
        }
      } else {
        findStart();
      }
    }

    Node* operator * () {
      return reinterpret_cast<Node*>(pos);
    }

    NodeList* insertBefore() {
      NodeList** gap = reinterpret_cast<NodeList**>(pos - gapSize);
      if (!*gap) {
        *gap = new NodeList();
      }
      return *gap;
    }

    friend class NodeList;
  };

  friend class iterator;

  iterator begin() {
    return iterator(buf + gapSize, pos);
  }

  iterator end() {
    return iterator(-1, -1);
  }

  iterator at(size_t pos) {
    iterator i = begin();
    for (size_t p = 0; p < pos; ++p) {
      ++i;
    }
    return i;
  }
};
