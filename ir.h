#include <cstdint>
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
    Deleted,
  };

  inline size_t realSize() const;

  const Type type;
  Node(const Type type) : type(type) {}

  Node(Node const & from):
      type(from.type) {
  }

  virtual void print(std::ostream& os) const = 0;
  friend std::ostream& operator<<(std::ostream& os, const Node& n) {
    n.print(os);
    return os;
  }

  virtual void adjustOffset(Node * n) {
    *(Node**)n = this;
  }

  /** Note that after calling this method, the node pointer should never be used again.
   */
  void eraseFromList();
};

class Constant : public Node {
 public:
  const int value;
  Constant(int value) : Node(Type::Constant), value(value) {}

  Constant(Constant const & from):
      Node(from),
      value(from.value) {
  }

  void print(std::ostream& os) const override {
    os << value;
  }
};


class Add : public Node {
    Node * l_;
    Node * r_;

 public:
  Add(Node* l, Node* r) : Node(Type::Add),
      l_(l),
      r_(r) {
  }

  Add(Add const & from):
      Node(from),
      l_(from.l_),
      r_(from.r_) {
  }


  Node* l() const {
    return l_;
  }

  Node* r() const {
    return r_;
  }

  void print(std::ostream& os) const override {
    os << *l() << " + " << *r();
  }

  void adjustOffset(Node* old_) override {
    Add* old = static_cast<Add*>(old_);
    l_ = *(Node**)old->l();
    r_ = *(Node**)old->r();
    Node::adjustOffset(old_);
  }
};

class Deleted : public Node {
public:
    const uint8_t deletedSize;

    void print(std::ostream& os) const override {
      os << "deleted(" << deletedSize << "X)";
    }
private:
    friend class Node;

    Deleted(Node * n, uint8_t size):
        Node(Type::Deleted),
        deletedSize(size) {
    }
};

inline void Node::eraseFromList() {
    new(this) Deleted(this, realSize());
}

size_t Node::realSize() const {
  switch(type) {
    case Type::Constant: return sizeof(Constant);
    case Type::Add: return sizeof(Add);
      case Type::Deleted:
          return ((Deleted*)this)->deletedSize;
  }
  assert(false);
  return -1;
}

#pragma pack(pop) // exact fit - no padding

class NodeList {
  static constexpr size_t defaultSize = 128*1024;
  static constexpr size_t gapSize = sizeof(NodeList*);

  uintptr_t buf;
  uintptr_t pos;

  bool full = false;
  NodeList* nextFree = nullptr;

  uintptr_t size;

  static constexpr unsigned gapsCacheSize = 8;
  NodeList* gapsCache[gapsCacheSize];
  unsigned numGaps = 0;

  NodeList* next = nullptr;


  NodeList & operator = (NodeList && from) {
      destructor();
      memcpy(this, &from, sizeof(NodeList));
      // signifies that when deleting from, we should not care about others associated with it
      from.buf = 0;
      return *this;
  }

  inline void* prepareInsert(size_t s) {
    if (nextFree && !nextFree->full) {
      return nextFree->prepareInsert(s);
    }

    uintptr_t next_pos = pos + s + gapSize;
    if (next_pos < buf + size) {
      void* res = (void*)pos;
      pos = next_pos;
      *(NodeList**)(pos - gapSize) = nullptr;
      return res;
    }

    return prepareInsertSlow(s);
  }

  void* prepareInsertSlow(size_t s) {
    full = true;

    NodeList* cur = this;
    while(cur->full && cur->next) {
      cur = cur->next;
    }

    if (cur == this) {
      next = nextFree = new NodeList();
      return next->prepareInsert(s);
    }

    nextFree = cur;
    return nextFree->prepareInsert(s);
  }

  // Bulk copy, to avoid doing insert(Node*) for every element
  static uintptr_t fixup(uintptr_t old_start,
                  uintptr_t old_end,
                  uintptr_t new_start) {

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
  }


 public:
  size_t totalSize() {
    size_t sum = size;

    if (numGaps > gapsCacheSize) {
      uintptr_t finger = buf;
      while (true) {
        NodeList** gap = reinterpret_cast<NodeList**>(finger);
        if (*gap)
          sum += (*gap)->totalSize();
        finger += gapSize;
        if (finger >= pos)
          break;
        auto n = (Node*)finger;
        finger += n->realSize();
      }
    } else {
      for (unsigned i = 0; i < numGaps; ++i) {
        sum += gapsCache[i]->totalSize();
      }
    }
    if (next)
      sum += next->totalSize();
    return sum;
  }

  NodeList(size_t initSize = defaultSize) : size(initSize) {
    buf = (uintptr_t)new char[initSize];

    *(NodeList**)buf = nullptr;
    pos = buf + gapSize;
  }

  void destructor() {
      if (numGaps > gapsCacheSize) {
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
          delete gapsCache[i];
        }
      }
      if (next) delete next;
      delete[] (char*)buf;
      buf = (uintptr_t) nullptr;
  }

  ~NodeList() {
      if (buf != (uintptr_t)nullptr)
          destructor();

  }

  template<typename NODE>
  NODE * append(NODE const & ins) {
      return new(prepareInsert(sizeof(NODE))) NODE(ins);
  }

  void flatten() {
     NodeList * flat = new NodeList(totalSize());

     auto i = begin();
     NodeList* cur = i.cur;
     uintptr_t bulkFixupStart = i.pos;
     uintptr_t bulkFixupEnd = i.pos;
     unsigned depth = 0;

     for (; !i.isEnd(); ++i) {
       if (cur != i.cur || i.worklist.size() != depth) {
         flat->pos = fixup(bulkFixupStart, bulkFixupEnd, flat->pos);
         depth = i.worklist.size();
         cur = i.cur;
         bulkFixupStart = i.pos;
       }
       bulkFixupEnd = i.pos;
     }
     flat->pos = fixup(bulkFixupStart, bulkFixupEnd, flat->pos);

     *this = std::move(*flat);
     //delete flat;
   }

  class iterator {
    NodeList* cur;
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

    iterator () {};

   public:
    static iterator theEnd() {
      iterator i;
      i.pos = i.end = -1;
      return i;
    }

    iterator(NodeList* cur) : cur(cur),
                              pos(cur->buf + gapSize),
                              end(cur->pos) {
        findStart();
    }

    bool operator != (iterator& other) {
      assert((int)other.pos == -1 && (int)other.end == -1);
      return !isEnd();
    }

    bool isEnd() {
      while (pos == end && !worklist.empty()) {
        popWorklist();
      }
      return worklist.empty() && pos == end;
    }

    void operator ++ () {
      if (cur->next && pos == end) {
        cur = cur->next;
        pos = cur->buf + gapSize;
        end = cur->pos;
      }
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
        if (p->numGaps < p->gapsCacheSize) {
          p->gapsCache[p->numGaps] = *gap;
        }
        p->numGaps++;
      }
      return *gap;
    }

    friend class NodeList;
  };

  friend class iterator;


  NodeList * insertPatchBefore(Node * i) {
      NodeList **  n = reinterpret_cast<NodeList**>((uintptr_t)i - gapSize);
      if (*n == nullptr) {
          *n = new NodeList();
          if (numGaps < gapsCacheSize)
              gapsCache[numGaps] = *n;
          ++numGaps;
      }
      return *n;
  }

  NodeList * insertPatchAfter(Node * i) {
      return nullptr;
  }

  NodeList* insertBefore(iterator i) {
    return i.insertBefore(this);
  }

  iterator begin() {
    return iterator(this);
  }

  iterator end() {
    return iterator::theEnd();
  }

  iterator at(size_t pos) {
    iterator i = begin();
    for (size_t p = 0; p < pos; ++p) {
      ++i;
    }
    return i;
  }
};
