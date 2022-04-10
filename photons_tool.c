#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<stdint.h>
#include<unistd.h>
#include<ctype.h>

#include "photons.h"
#include "photons_rle.h"

void usage(char *progname) {
  fprintf(stderr, "Usage: %s -f <file> [-P <preview file>] [-L <layer file prefix>] [-s <starting exposure> [-e <ending exposure>]]\n", progname);
  fprintf(stderr, "      [-o <layer offset (not counting base layers)] [-I <individual settings value>] [-N <normal exposure time>]\n");
  fprintf(stderr, "      [-l <file with layer paths>] [-F flags]\n");
#if 0
  fprintf(stderr, "[-M <log modifier> (remaps gray values)]\n");
#endif
}

// We have to assume that any blob we read from the file will fit in one of these
// buffers, but also that a decoded raw image will fit. Just make it large enough
// and never put buffers on the stack.
#define BUFSIZE (1024 * 1024 * 4)

#define FLAG_DO_NOT_DECODE_LAYERS 0x0001

int main(int argc, char *argv[]) {
  int i, offset=0, o;
  FILE *photonsFile=NULL;
  FILE *outfile;
  char *previewFile=NULL;
  char *layerPrefix=NULL;
  int individualParams=0;
  int setIndividualParams=0;
  int headerChanged=0;
  int currentLayer;
  uint32_t flags=0;
  int encodedImageSize;
  int rawImageSize;
  float normalExposureTime=-1;
  static char filenamebuf[2048];
  float startExp=-1, endExp=-1;
  static unsigned char encodedBuf[BUFSIZE];
  static unsigned char rawBuf[BUFSIZE];
  struct photons_fileheader pfh;
  struct photons_header ph;
  struct preview_header pvh;
  struct layersdef_header ldh;
  struct layersdef_layer ldl, bottomLayer_def, normalLayer_def;
  FILE *layerListFile=NULL;
  FILE *layerFile;
#if 0
  float logMod=-1;
#endif
  int currentLayerAddress, nextLayerAddress=0, layerDataAddress;
  while ((o=getopt(argc, argv, "f:P:L:s:e:o:I:N:l:F:"))!=-1) { // M:
      switch (o)
	{
	case 'f':
	  if (!(photonsFile=fopen(optarg,"r+"))) {
	    perror(optarg);
	    return -1;
	  }
	  break;
	case 'P':
	  previewFile = optarg;
	  break;
	case 'L':
	  layerPrefix = optarg;
	  break;
	case 's':
	  startExp=atof(optarg);
	  break;
	case 'e':
	  endExp=atof(optarg);
	  break;
#if 0	 
	case 'M':
	  logMod=atof(optarg);
	  break;
#endif
	case 'o':
	  offset=atoi(optarg);
	  break;
	case 'I':
	  individualParams=strtol(optarg, NULL, 0);
	  setIndividualParams=1;
	  break;
	case 'N':
	  normalExposureTime=atof(optarg);
	  break;
	case 'F':
	  flags=strtol(optarg, NULL, 0);
	  break;
	case 'l':
	  if (!(layerListFile=fopen(optarg,"r"))) {
	    perror(optarg);
	    return -1;
	  }
	  break;
	default:
	  usage(argv[0]);
	  return -1;
	  break;
	}
    }
  if (!photonsFile) {
    usage(argv[0]);
    return -1;
  }

  // ****** Read stuff
  if (fread(&pfh, sizeof(pfh), 1, photonsFile) != 1) {
    fprintf(stderr, "Short read on file header.\n");
    return -10;
  }
  fseek(photonsFile, pfh.headerAddress, SEEK_SET);
  if (fread(&ph, sizeof(ph), 1, photonsFile) != 1) {
    fprintf(stderr, "Short read on header.\n");
    return -10;
  }
  fseek(photonsFile, pfh.previewAddress, SEEK_SET);
  if (fread(&pvh, sizeof(pvh), 1, photonsFile) != 1) {
    fprintf(stderr, "Short read on preview header.\n");
    return -10;
  }
  fseek(photonsFile, pfh.layerDefAddress, SEEK_SET);
  if (fread(&ldh, sizeof(ldh), 1, photonsFile) != 1) {
    fprintf(stderr, "Short read on layerdefs header.\n");
    return -10;
  }

  // ****** Print info
  printFileHeader(stdout, &pfh);
  printHeader(stdout, &ph);
  printPreviewHeader(stdout, &pvh);
  printLayerDefHeader(stdout, &ldh);
  for (i=0; i<ldh.nlayers; i++) {
    fprintf(stdout, "---------\nLayer %d\n", i);
    fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh) + i * sizeof(ldl), SEEK_SET);
    if (fread(&ldl, sizeof(ldl), 1, photonsFile) != 1) {
      fprintf(stderr, "Short read on layerdefs layer header.\n");
      return -10;
    }
    printLayersDefLayer(stdout, &ldl);
  }
  
  // ****** Extract stuff

  // Extract preview. This is not run-length-encoded
  if (previewFile != NULL) {
    fseek(photonsFile, pfh.previewAddress + sizeof(pvh), SEEK_SET);
    if (!(outfile = fopen(previewFile, "w"))) {
      perror(previewFile);
      return -2;
    }
    if (fread(rawBuf, 1, pvh.length - sizeof(pvh), photonsFile) != (pvh.length - sizeof(pvh))) {
      fprintf(stderr, "Short read on preview.\n");
      return -10;
    }
    fwrite(rawBuf, 1, pvh.length - sizeof(pvh), outfile);
    fclose(outfile);
  }

  // Extract all the layers. These ARE run-length-encoded
  if (layerPrefix != NULL) {
    for (i=0; i<ldh.nlayers; i++) {
      fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh) + i * sizeof(ldl), SEEK_SET);
      if (fread(&ldl, sizeof(ldl), 1, photonsFile) != 1) {
	fprintf(stderr, "Short read on layerdefs layer header.\n");
	return -10;
      }
      sprintf(filenamebuf, "%s_%04d.bin", layerPrefix, i);
      if (!(outfile = fopen(filenamebuf, "w"))) {
	perror(filenamebuf);
	return -2;
      }
      fseek(photonsFile, ldl.address, SEEK_SET);
      if (fread(encodedBuf, 1, ldl.datalen, photonsFile) != ldl.datalen) {
	fprintf(stderr, "Short read on layer image.\n");
	return -10;
      }
      if (flags & FLAG_DO_NOT_DECODE_LAYERS) {
	fwrite(encodedBuf, 1, ldl.datalen, outfile);
	printf("Extracted encoded layer %d as %s, size %d\n", i, filenamebuf, ldl.datalen);
      } else {
	rawImageSize = rleDecode(encodedBuf, ldl.datalen, rawBuf, sizeof(rawBuf));
	fwrite(rawBuf, 1, rawImageSize, outfile);
	printf("Extracted layer %d as %s, size %d\n", i, filenamebuf, rawImageSize);
      }
      fclose(outfile);
    }
  }

  // ****** Do changes

  // Set the "Use Individual Params" value
  if (setIndividualParams) {
    ph.individualParameters = individualParams;
    headerChanged=1;
  }

  // Set the normal exposure
  if (normalExposureTime > -1) {
    ph.normalExposureTime = normalExposureTime;
    headerChanged=1;
  }

  // Write out the modified header
  if (headerChanged) {
    fseek(photonsFile, pfh.headerAddress, SEEK_SET);
    fwrite(&ph, sizeof(ph), 1, photonsFile);
  }

  // Set individual exposure times on layers
  if (startExp > 0) {
    if (endExp < 0) {
      endExp = startExp;
    }
    for (i=ph.bottomLayers+offset; i<ldh.nlayers; i++) {
      fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh) + i * sizeof(ldl), SEEK_SET);
      if (fread(&ldl, sizeof(ldl), 1, photonsFile) != 1) {
	fprintf(stderr, "Short read on layerdefs layer header.\n");
	return -10;
      }
								     
      if (i >= ph.bottomLayers + offset) {
	ldl.expTime = startExp - (startExp - endExp) * (float)(i - offset - ph.bottomLayers)/(ldh.nlayers - offset - ph.bottomLayers);
	fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh) + i * sizeof(ldl), SEEK_SET);
	fwrite(&ldl, sizeof(ldl), 1, photonsFile);
      }
    }
  }

  // Replace all layers with the ones from the layer list
  if (layerListFile) {
    // Read first LayersDef  into bottomLayer_def
    fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh), SEEK_SET);
    if (fread(&bottomLayer_def, sizeof(bottomLayer_def), 1, photonsFile) != 1) {
      fprintf(stderr, "Short read on layerdefs layer header - bottom layer.\n");
      return -10;
    }

    // Read first LayersDef after bottomLayers  into normalLayer_def
    fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh) + ph.bottomLayers * sizeof(ldl), SEEK_SET);
    if (fread(&normalLayer_def, sizeof(normalLayer_def), 1, photonsFile) != 1) {
      fprintf(stderr, "Short read on layerdefs layer header - non-bottom layer.\n");
      return -10;
    }
    // Seek to the first layerdef
    fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh), SEEK_SET);
    // Loop over list of infiles:
    currentLayer=0;
    while(fgets(filenamebuf, sizeof(filenamebuf), layerListFile)) {
      //    write saved LayersDef - either bottomLayer_def or normalLayer_def
      if (currentLayer < ph.bottomLayers) {
	fwrite(&bottomLayer_def, sizeof(bottomLayer_def), 1, photonsFile);
      } else {
	fwrite(&normalLayer_def, sizeof(normalLayer_def), 1, photonsFile);
      }
      currentLayer++;
    }
    layerDataAddress = ftell(photonsFile);
    // Update nlayers in LayersDefHeader
    ldh.nlayers = currentLayer;
    // Seek to LayersDefHeader
    fseek(photonsFile, pfh.layerDefAddress, SEEK_SET);
    // Write LayersDefHeader
    fwrite(&ldh, sizeof(ldh), 1, photonsFile);
    // Rewind and loop over list of infiles:
    fseek(layerListFile, 0, SEEK_SET);
    fseek(photonsFile, layerDataAddress, SEEK_SET);
    currentLayer=0;
    while(fgets(filenamebuf, sizeof(filenamebuf), layerListFile)) {
      while (isspace(filenamebuf[strlen(filenamebuf)-1])) {
	filenamebuf[strlen(filenamebuf) - 1] = '\0';
      }
      //    Save address in currentLayerAddress
      currentLayerAddress = ftell(photonsFile);
      //    Open layer file
      if (!(layerFile = fopen(filenamebuf, "r"))) {
	perror(filenamebuf);
	return -3;
      }
      //    RLE encode into photons file
      //    Save length in encodedImageSize
      rawImageSize=fread(rawBuf, 1, sizeof(rawBuf), layerFile);
      fclose(layerFile);
      encodedImageSize = rleEncode(rawBuf, rawImageSize, encodedBuf, sizeof(encodedBuf));
      fwrite(encodedBuf, 1, encodedImageSize, photonsFile);
      printf("Added %s to file, encoded size %d.\n", filenamebuf, encodedImageSize);
      //    Save new address in nextLayerAddress
      nextLayerAddress = ftell(photonsFile);
      //    Seek to correct LayersDef
      fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh) + currentLayer * sizeof(ldl), SEEK_SET);
      //    read LayersDef
      if (fread(&ldl, sizeof(ldl), 1, photonsFile) != 1) {
	fprintf(stderr, "Short read on layerdefs layer header.\n");
	return -10;
      }

      //    Set address and datalen
      ldl.address = currentLayerAddress;
      ldl.datalen = encodedImageSize;
      //    write LayersDef
      fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh) + currentLayer * sizeof(ldl), SEEK_SET);
      fwrite(&ldl, sizeof(ldl), 1, photonsFile);
      //    seek to nextLayerAddress
      fseek(photonsFile, nextLayerAddress, SEEK_SET);
      currentLayer++;
    }
    // Truncate file to <nextLayer>
    fflush(photonsFile);
    if (ftruncate(fileno(photonsFile), nextLayerAddress) != 0) {
      perror ("ftruncate()");
      return -11;
    }
    fclose(layerListFile);
  }

#if 0
  // This should not be done in core. It should be done by extracting the layers to a work directory,
  // and then reading them back and modifying them.
  if (logMod > 0) {
    for (i=ph.bottomLayers+offset; i<ldh.nlayers; i++) {
      fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh) + i * sizeof(ldl), SEEK_SET);
      fread(&ldl, sizeof(ldl), 1, photonsFile);
      
    }
  }
#endif
  fclose(photonsFile);
  return 0;
}
