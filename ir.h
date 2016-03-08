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

  static constexpr unsigned gapsStore = 4;
  NodeList* gaps[gapsStore];
  unsigned numGaps = 0;

 public:
  NodeList() {

    buf = (uintptr_t)malloc(size);
    *(NodeList**)buf = nullptr;
    pos = buf + gapSize;
  }

  ~NodeList() {
    if (numGaps > gapsStore) {
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
    } else {
      for (unsigned i = 0; i < numGaps; ++i) {
        delete gaps[i];
      }
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

    // Bulk copy, to avoid doing insert(Node*) for every element
    auto fixup = [](uintptr_t old_start,
                    uintptr_t old_end,
                    uintptr_t new_start) -> uintptr_t {

      Node* last = (Node*) old_end;
      size_t last_size = last->realSize();

      size_t s = old_end - old_start + last_size;
      memcpy((void*)new_start, (void*)old_start, s);

      uintptr_t finger_new = new_start;
      uintptr_t finger_old = old_start;

      while (finger_old <= old_end) {
        Node* old = (Node*)finger_old;
        Node* copy = (Node*)finger_new;
        size_t s = copy->realSize();
        copy->adjustOffset(old);
        *((NodeList**)(finger_new - gapSize)) = nullptr;
        finger_old += s + gapSize;
        finger_new += s + gapSize;
      }

      *((NodeList**)(finger_new - gapSize)) = nullptr;

      return finger_new;
    };

    auto i = begin();
    uintptr_t bulkFixupStart = i.pos;
    uintptr_t bulkFixupEnd = i.pos;
    unsigned depth = 0;

    for (; !i.isEnd(); ++i) {
      if (i.worklist.size() != depth) {
        flat->pos = fixup(bulkFixupStart, bulkFixupEnd, flat->pos);
        depth = i.worklist.size();
        bulkFixupStart = i.pos;
      }
      bulkFixupEnd = i.pos;
    }
    flat->pos = fixup(bulkFixupStart, bulkFixupEnd, flat->pos);

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
      return !isEnd();
    }

    bool isEnd() {
      while (pos == end && !worklist.empty()) {
        popWorklist();
      }
      return worklist.empty() && pos == end;
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

   private:
    inline NodeList* insertBefore(NodeList* p) {
      NodeList** gap = reinterpret_cast<NodeList**>(pos - gapSize);
      if (!*gap) {
        *gap = new NodeList();
        if (p->numGaps < p->gapsStore) {
          p->gaps[p->numGaps] = *gap;
        }
        p->numGaps++;
      }
      return *gap;
    }

    friend class NodeList;
  };

  friend class iterator;

  NodeList* insertBefore(iterator i) {
    return i.insertBefore(this);
  }

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
