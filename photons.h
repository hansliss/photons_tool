#ifndef PHOTONS_H
#define PHoTONS_H

#include<stdint.h>

struct photons_fileheader {
  char sectionId[12];
  uint8_t versionMinor;
  uint8_t versionMajor;
  uint16_t dummy;
  uint32_t numAreas;
  uint32_t headerAddress;
  uint32_t filler1;
  uint32_t previewAddress;
  uint32_t filler2;
  uint32_t layerDefAddress;
  uint32_t filler3;
  uint32_t layerAddress;
};

struct photons_header {
  char sectionId[12];
  uint32_t headerLen;
  float xyPixel;
  float zThickness;
  float normalExposureTime;
  float offTime;
  float bottomExposureTime;
  float bottomLayers;
  float zLiftHeight;
  float zLiftSpeed;  
  float zDropSpeed;
  float totalVolume;
  uint32_t antiAliasLevel;
  uint32_t resX;
  uint32_t resY;
  float weight;
  float price;
  uint32_t resinType;
  uint32_t individualParameters;
  uint32_t filler1;
  uint32_t filler2;
  uint32_t filler3;
};

struct preview_header {
  char sectionId[12];
  uint32_t length;
  uint32_t width;
  uint32_t mark;
  uint32_t height;
};

struct layersdef_header {
  char sectionId[12];
  uint32_t length;
  uint32_t nlayers;
};

struct layersdef_layer {
  uint32_t address;
  uint32_t datalen;
  float zHeight;
  float liftSpeed;
  float expTime;
  uint8_t reserved[12];
};

void printFileHeader(FILE *f, struct photons_fileheader *fh);
void printHeader(FILE *f, struct photons_header *h);
void printPreviewHeader(FILE *f, struct preview_header *ph);
void printLayerDefHeader(FILE *f, struct layersdef_header *ldh);
void printLayersDefLayer(FILE *f, struct layersdef_layer *ldl);


#endif
