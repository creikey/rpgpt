#include <stdlib.h>

void assert_less(int x, int y) {
  if (x >= y) {
    abort();
  }
}

int main() {
  assert_less(10, 20);
  assert_less(30, 20);
}