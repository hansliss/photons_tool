#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<stdint.h>
#include<unistd.h>
#include<ctype.h>
#include<math.h>

#define BUFSIZE 1024 * 1024 * 4

void usage(char *progname) {
  fprintf(stderr, "Usage: %s -f <file> -s <scale factor>\n", progname);
}

int main(int argc, char *argv[]) {
  int o;
  float scaleFactor=1;
  int map[16];
  FILE *file=NULL;
  static unsigned char buf[BUFSIZE];
  int nbytes, pos=0;
  while ((o=getopt(argc, argv, "f:s:"))!=-1) {
      switch (o)
	{
	case 'f':
	  if (!(file = fopen(optarg, "r+"))) {
	    perror(optarg);
	    return -2;
	  }
	  break;
	case 's':
	  scaleFactor = atof(optarg);
	  break;
	default:
	  usage(argv[0]);
	  return -1;
	  break;
	}
  }
  if (!file) {
    usage(argv[0]);
    return -1;
  }
  float v = 15 / (logf(scaleFactor * 15) / logf(15));
  map[0] = 0;
  for (float i=1; i<16; i++) {
    map[(int)i] = (int)(0.5 + v * logf(scaleFactor * i) / logf(15));
    printf("Map %d to %d\n", (int)i, map[(int)i]);
  }
  
  while ((nbytes = fread(buf, 1, BUFSIZE, file)) > 0) {
    for (int i=0; i<nbytes; i++) {
      buf[i] = (map[(buf[i] & 0xF0) >> 4] << 4) | (map[buf[i] & 0x0F]);
    }
    fseek(file, pos, SEEK_SET);
    fwrite(buf, 1, nbytes, file);
    pos += nbytes;
  }
  fclose(file);
  return 0;
}
