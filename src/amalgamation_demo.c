/* auto-generated on Tue 10 Jun 2025 08:08:02 PM PDT. Do not edit! */

#include <stdio.h>
#include "roaring.cpp"
int main() {
  roaring_bitmap_t *r1 = roaring_bitmap_create();
  for (uint32_t i = 100; i < 1000; i++) roaring_bitmap_add(r1, i);
  printf("cardinality = %d\n", (int) roaring_bitmap_get_cardinality(r1));
  roaring_bitmap_free(r1);
  return 0;
}

