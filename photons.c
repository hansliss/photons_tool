#include<stdio.h>
#include<string.h>

#include "photons.h"


void printFileHeader(FILE *f, struct photons_fileheader *fh) {
  char si[13];
  memcpy(si, fh->sectionId, 12);
  si[12] = '\0';
  fprintf(f, "File header:      %s\n", si);
  fprintf(f, "Version:          %d.%d\n", fh->versionMajor, fh->versionMinor);
  fprintf(f, "Number of areas:  %d\n", fh->numAreas);
  fprintf(f, "Header address:   %d\n", fh->headerAddress);
  fprintf(f, "(filler 1):       %d\n", fh->filler1);
  fprintf(f, "Preview address:  %d\n", fh->previewAddress);
  fprintf(f, "Unknown area:     %d\n", fh->unknown1Address);
  fprintf(f, "LayerDef address: %d\n", fh->layerDefAddress);
  fprintf(f, "(filler 3):       %d\n", fh->filler3);
  fprintf(f, "Layer address:    %d\n", fh->layerAddress);
}

void printHeader(FILE *f, struct photons_header *h) {
  char si[13];
  memcpy(si, h->sectionId, 12);
  si[12] = '\0';
  fprintf(f, "Header:           %s\n", si);
  fprintf(f, "Header length:    %d\n", h->headerLen);
  fprintf(f, "XY Pixel:         %f\n", h->xyPixel);
  fprintf(f, "Z thickness:      %f\n", h->zThickness);
  fprintf(f, "Normal exposure:  %f\n", h->normalExposureTime);
  fprintf(f, "Off time:         %f\n", h->offTime);
  fprintf(f, "Bottom exposure:  %f\n", h->bottomExposureTime);
  fprintf(f, "Bottom layers:    %f\n", h->bottomLayers);
  fprintf(f, "Z lift height:    %f\n", h->zLiftHeight);
  fprintf(f, "Z lift speed:     %f\n", h->zLiftSpeed);
  fprintf(f, "Z drop speed:     %f\n", h->zDropSpeed);
  fprintf(f, "Total volume:     %f\n", h->totalVolume);
  fprintf(f, "Antialias level:  %d\n", h->antiAliasLevel);
  fprintf(f, "X resolution:     %d\n", h->resX);
  fprintf(f, "Y resolution:     %d\n", h->resY);
  fprintf(f, "Weight:           %04X\n", h->weight);
  fprintf(f, "Price:            %f\n", h->price);
  fprintf(f, "Resin type:       %d\n", h->resinType);
  fprintf(f, "Use indiv. para.: 0x%04X\n", h->individualParameters);
  fprintf(f, "Print time (s):   %d\n", h->printTime);
  fprintf(f, "(filler 2):       %d\n", h->filler2);
  fprintf(f, "(filler 3):       %d\n", h->filler3);
}

void printPreviewHeader(FILE *f, struct preview_header *ph) {
  char si[13];
  memcpy(si, ph->sectionId, 12);
  si[12] = '\0';
  fprintf(f, "Header:           %s\n", si);
  fprintf(f, "Header length:    %d\n", ph->length);
  fprintf(f, "Width:            %d\n", ph->width);
  fprintf(f, "mark:             %d\n", ph->mark);
  fprintf(f, "height:           %d\n", ph->height);
}
    
void printUnknown1Header(FILE *f, struct unknown1_header *u1h) {
  fprintf(f, "(Unknown area 1)\n");
  fprintf(f, "(filler 1):       %d\n", u1h->filler1);
  fprintf(f, "Length:           %d%s\n", u1h->length, (u1h->length != 16)?" (not 16!)":"");
  for (int i=0; i<16; i++) {
    fprintf(f, "Value %02d: 0x%02X\n", i, u1h->val[i]);
  }
}

void printLayerDefHeader(FILE *f, struct layersdef_header *ldh) {
  char si[13];
  memcpy(si, ldh->sectionId, 12);
  si[12] = '\0';
  fprintf(f, "LayersDef Header: %s\n", si);
  fprintf(f, "LayersDef length: %d\n", ldh->length);
  fprintf(f, "Number of layers  %d\n", ldh->nlayers);
}

void printLayersDefLayer(FILE *f, struct layersdef_layer *ldl) {
  fprintf(f, "Layer address:          %d\n", ldl->address);
  fprintf(f, "Layer datalen:          %d\n", ldl->datalen);
  fprintf(f, "Layer Z height:         %f\n", ldl->zHeight);
  fprintf(f, "Layer lift speed:       %f\n", ldl->liftSpeed);
  fprintf(f, "Layer exposure time:    %f\n", ldl->expTime);
  fprintf(f, "Layer thickness:        %f\n", ldl->layerThickness);
  fprintf(f, "Layer non-black pixels: %d\n", ldl->nonBlackPixels);
  fprintf(f, "Layer filler 2:         %d\n", ldl->filler2);
}
