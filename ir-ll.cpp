#include "ir-ll.h"

void test(bool print = false) {
  auto bb = new NodeList();
  auto* c1 = bb->push_back(new Constant(1));
  auto* c2 = bb->push_back(new Constant(2));
  auto* a = bb->push_back(new Add(c1, c2));

  if (print) {
    for (auto i : *bb)
      std::cout << *i << "\n";
    std::cout << "===============\n";
  }

  auto patch = bb->at(1);
  Node* c3 = bb->insert(patch, new Constant(3));

  if (print) {
    for (auto i : *bb)
      std::cout << *i << "\n";
    std::cout << "===============\n";
  }

  auto patch2 = bb->at(1);
  auto c4 = bb->insert(patch2, new Constant(4));

  if (print) {
    for (auto i : *bb)
      std::cout << *i << "\n";
    std::cout << "===============\n";
  }

  auto patch3 = bb->at(3);
  auto a1 = bb->insert(patch3, new Add(c3, c4));
  bb->push_back(new Add(a1, a));

  if (print) {
    for (auto i : *bb)
      std::cout << *i << "\n";
    std::cout << "===============\n";
  }

  auto patch4 = bb->at(6);
  for (int i = 0; i < 100000; ++i)
    bb->insert(patch4, new Constant(i));

  int count = 0;
  for (auto i : *bb) count++;
  if (print)
    std::cout << count << "\n";

  delete bb;
}

int main() {
  test(true);
  for (int i = 0; i < 500; ++i) {
    test();
  }
  return 0;
}
