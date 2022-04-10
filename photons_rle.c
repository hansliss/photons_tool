#include<stdio.h>

int rleDecode(unsigned char *inbuf, int inbuflen, unsigned char *outbuf, int outbuflen) {
  int c1, c2, ind=0, i, byte=0, bit=4;
  int outpointer=0;
  while (ind < inbuflen) {
    c1 = inbuf[ind++];
    if (ind < inbuflen && (((c1 & 0xF0) == 0) || ((c1 & 0xF0) == 0xF0))) {
      c2 = inbuf[ind++];
      for (i=0; i < (((c1 & 0x0F) << 8) | c2); i++) {
	byte |= ((c1 & 0xF0) >> 4) << bit;
	if (bit == 0) {
	  if (outpointer >= outbuflen) {
	    return -1;
	  }
	  outbuf[outpointer++] = byte;
	  bit=4;
	  byte=0;
	} else {
	  bit-=4;
	}
      }
    } else {
      for (i=0; i < (c1 & 0x0F); i++) {
	byte |= ((c1 & 0xF0) >> 4) << bit;
	if (bit == 0) {
	  if (outpointer >= outbuflen) {
	    return -1;
	  }
	  outbuf[outpointer++] = byte;
	  bit=4;
	  byte=0;
	} else {
	  bit-=4;
	}
      
      }
    }
  }
  return outpointer;
}

int nextnibble(unsigned char *inbuf, int inbuflen, int *inbufpointer, int *bit) {
  int nibble;
  if ((*inbufpointer) >= inbuflen) {
    return EOF;
  }
  nibble = (inbuf[*inbufpointer] & (0x0f << (*bit))) >> (*bit);
  (*bit) = 4 - (*bit);
  if ((*bit) == 4) {
    (*inbufpointer)++;
  }
  return nibble;
}

int emitValue(int gray, int currentCount, unsigned char *outbuf, int outbuflen, int *outbufpointer) {
  int outchars = 0;
  if (gray == 0 || gray == 0x0F) {
    while (currentCount > 0x0fff) {
      if ((*outbufpointer) <= outbuflen-2) {
	outbuf[(*outbufpointer)++] = (gray << 4) | 0x0f;
	outbuf[(*outbufpointer)++] = 0xff;
      }
      currentCount -= 0x0fff;
      outchars += 2;
    }
    if ((*outbufpointer) <= outbuflen-2) {
      outbuf[(*outbufpointer)++] = (gray << 4) | ((currentCount & 0x0f00) >> 8);
      outbuf[(*outbufpointer)++] = currentCount & 0xff;
    }
    outchars += 2;
  } else {
    while (currentCount > 0x0f) {
      if ((*outbufpointer) <= outbuflen-1) {
	outbuf[(*outbufpointer)++] = (gray << 4) | 0x0f;
      }
      currentCount -= 0x0f;
      outchars++;
    }
    if ((*outbufpointer) <= outbuflen-1) {
      outbuf[(*outbufpointer)++] = (gray << 4) | (currentCount & 0x0f);
    }
    outchars++;
  }
  return outchars;
}

int rleEncode(unsigned char *inbuf, int inbuflen, unsigned char *outbuf, int outbuflen) {
  int gray=-1, newGray;
  int currentCount=0;
  int outchars=0;
  int inbufpointer=0, outbufpointer=0;
  int bit=4;
  while((newGray = nextnibble(inbuf, inbuflen, &inbufpointer, &bit)) != EOF) {
    if (newGray != gray) {
      if (gray != -1 && currentCount != 0) {
	outchars += emitValue(gray, currentCount, outbuf, outbuflen, &outbufpointer);
      }
      gray = newGray;
      currentCount = 1;
    } else {
      currentCount++;
    }
  }
  if (gray != -1 && currentCount != 0) {
    outchars += emitValue(gray, currentCount, outbuf, outbuflen, &outbufpointer);
  }
  return outbufpointer;
}
