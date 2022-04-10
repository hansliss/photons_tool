#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<stdint.h>
#include<unistd.h>
#include<ctype.h>

int nextnibble(FILE *infile) {
  static int inchar;
  static int bit = 4;
  int nibble;
  if (bit == 4) {
    if ((inchar = getc(infile)) == EOF) {
      return -1;
    }
  }
  nibble = (inchar & (0x0f << bit)) >> bit;
  bit = 4 - bit;
  return nibble;
}

int main(int argc, char *argv[]) {
  int counts[16];
  int nibble;
  memset(counts, 0, sizeof(counts));
  while((nibble = nextnibble(stdin)) != EOF) {
    counts[nibble]++;
  }
  for (int i=0; i<16; i++) {
    printf("%1X: %d\n", i, counts[i]);
  }
  return 0;
}
