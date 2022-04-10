#ifndef PHOTONS_RLE_H
#define PHOTONS_RLE_H

#include<stdio.h>

int rleDecode(unsigned char *inbuf, int inbuflen, unsigned char *outbuf, int outbuflen);
int rleEncode(unsigned char *inbuf, int inbuflen, unsigned char *outbuf, int outbuflen);
#endif
