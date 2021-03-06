#include "ir.h"

void test(bool print = false) {
 // print = true;
  auto bb = new NodeList();
  auto* c1 = bb->append(Constant(1));
  auto* c2 = bb->append(Constant(2));
  auto* a = bb->append(Add(c1, c2));
  c1->eraseFromList();

  if (print) {
    for (auto i : *bb)
      std::cout << *i << std::endl;
    std::cout << "===============\n";
  }

  //auto patch = bb->insertBefore(bb->at(1));
  auto patch = bb->insertPatchBefore(c2);


  auto c3 = patch->append(Constant(3));

  if (print) {
    for (auto i : *bb)
      std::cout << *i << "\n";
    std::cout << "===============\n";
  }

  auto patch2 = bb->insertBefore(bb->at(1));
  auto c4 = patch2->append(Constant(4));

  if (print) {
    for (auto i : *bb)
      std::cout << *i << "\n";
    std::cout << "===============\n";
  }

  auto patch3 = bb->insertBefore(bb->at(3));
  auto a1 = patch3->append(Add(c3, c4));
  bb->append(Add(a1, a));

  if (print) {
    for (auto i : *bb)
      std::cout << *i << "\n";
    std::cout << "===============\n";
  }

  bb->flatten();

  if (print) {
    for (auto i : *bb)
      std::cout << *i << "\n";
    std::cout << "===============\n";
  }

  auto patch4 = bb->insertBefore(bb->at(6));
  for (int i = 0; i < 40000; ++i) {
    patch4->append(Add(
        patch4->append(Constant(i)),
        patch4->append(Constant(i))));
  }

  bb->flatten();

  int count = 0;
  // Simulate an analysis:
  for (auto i : *bb) {
    if (i->type == Node::Type::Add) {
      Node * l = ((Add*)i)->l();
      if (l->type == Node::Type::Constant) {
        count += ((Constant*)l)->value;
      }
    }
  }
  if (print)
    std::cout << count << "\n";

  delete bb;
}

using namespace std;

int main(int argc, char *argv[]) {
  // test(true);

  assert(argc == 2);

  int count = atoi(argv[1]);

  clock_t begin = clock();

  for (int i = 0; i < count; ++i) {
    test();
  }

  clock_t end = clock();
  double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;

  std::cerr << "Varray      (" << count << ") took : "
            << elapsed_secs << "\n";

  return 0;
}
