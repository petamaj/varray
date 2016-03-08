#include "ir.h"


void test(bool print = false) {
  auto bb = new NodeList();
  auto* c1 = bb->insert<Constant>(1);
  auto* c2 = bb->insert<Constant>(2);
  auto* a = bb->insert<Add>(c1, c2);

  if (print) {
    for (auto i : *bb)
      std::cout << *i << "\n";
    std::cout << "===============\n";
  }

  auto patch = bb->at(1).insertBefore();
  auto c3 = patch->insert<Constant>(3);

  if (print) {
    for (auto i : *bb)
      std::cout << *i << "\n";
    std::cout << "===============\n";
  }

  auto patch2 = bb->at(1).insertBefore();
  auto c4 = patch2->insert<Constant>(4);

  if (print) {
    for (auto i : *bb)
      std::cout << *i << "\n";
    std::cout << "===============\n";
  }

  auto patch3 = bb->at(3).insertBefore();
  auto a1 = patch3->insert<Add>(c3, c4);
  bb->insert<Add>(a1, a);

  if (print) {
    for (auto i : *bb)
      std::cout << *i << "\n";
    std::cout << "===============\n";
  }

  auto flat = bb->flatten();
  delete bb;

  if (print) {
    for (auto i : *flat)
      std::cout << *i << "\n";
    std::cout << "===============\n";
  }

  auto patch4 = flat->at(6).insertBefore();
  for (int i = 0; i < 100000; ++i)
    patch4->insert<Constant>(i);

  auto flat2 = flat->flatten();
  delete flat;

  int count = 0;
  for (auto i : *flat2) count++;
  if (print)
    std::cout << count << "\n";

  delete flat2;
}

using namespace std;

int main() {
//  test(true);

  clock_t begin = clock();

  for (int i = 0; i < 500; ++i) {
    test();
  }

  clock_t end = clock();
  double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;

  std::cerr << "Varray took : " << elapsed_secs << "\n";

  return 0;
}
