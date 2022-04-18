#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<stdint.h>
#include<unistd.h>
#include<ctype.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<math.h>
#include<png.h>

#include "photons.h"
#include "photons_rle.h"

void usage(char *progname) {
  fprintf(stderr, "Usage: %s -f <file> [-P <preview file>] [-L <layer file prefix>] [-s <starting exposure> -e <ending exposure>]\n", progname);
  fprintf(stderr, "      [-o <layer offset (not counting base layers)] [-I <individual settings value>] [-N <normal exposure time>]\n");
  fprintf(stderr, "      [-l <file with layer paths>] [-F flags] [-M <log modifier> (remaps gray values, requires work dir)]\n");
  fprintf(stderr, "      [-W <work dir>] [-G (count grey levels)] [-B <bottom exposure>] [-T <layer thickness>]\n");
  fprintf(stderr, "      [-X <exposure steps> (generates horizontal exposure test after <offset> layers - requires work dir)]\n");
}

// We have to assume that any blob we read from the file will fit in one of these
// buffers, but also that a decoded raw image will fit. Just make it large enough
// and never put buffers on the stack.
#define BUFSIZE (1024 * 1024 * 8)

#define FLAG_DO_NOT_DECODE_LAYERS 0x0001
#define FLAG_BITMAPS_AS_PNG 0x0002

int nonBlackPixels(FILE *photonsFile, struct layersdef_layer *ldl) {
  int nonBlack=0, rawImageSize;
  static unsigned char encodedBuf[BUFSIZE];
  static unsigned char rawBuf[BUFSIZE];
  fseek(photonsFile, ldl->address, SEEK_SET);
  if (fread(encodedBuf, 1, ldl->datalen, photonsFile) != ldl->datalen) {
    fprintf(stderr, "Short read on layer image.\n");
    return -10;
  }
  rawImageSize = rleDecode(encodedBuf, ldl->datalen, rawBuf, sizeof(rawBuf));
  for (int i=0; i<rawImageSize; i++) {
    if (rawBuf[i] & 0xf0) {
      nonBlack++;
    }
    if (rawBuf[i] & 0x0f) {
      nonBlack++;
    }
  }
  return nonBlack;
}

int main(int argc, char *argv[]) {
  int i, offset=0, o;
  FILE *photonsFile=NULL;
  FILE *outfile;
  char *previewFile=NULL;
  char *layerPrefix=NULL;
  char *layerSuffix="bin";
  char *workDirName=NULL;
  struct stat workDirStat;
  int individualParams=0;
  int setIndividualParams=0;
  int headerChanged=0;
  int currentLayer;
  uint32_t flags=0;
  int encodedImageSize;
  int rawImageSize;
  int countGreys=0;
  float normalExposureTime=-1;
  float bottomExposureTime=-1;
  float layerThickness=-1;
  static char filenamebuf[2048];
  float startExp=-1, endExp=-1;
  int exposureSteps=-1;
  static unsigned char encodedBuf[BUFSIZE];
  static unsigned char rawBuf[BUFSIZE];
  struct photons_fileheader pfh;
  struct photons_header ph;
  struct preview_header pvh;
  struct unknown1_header u1h;
  struct layersdef_header ldh;
  struct layersdef_layer ldl, bottomLayer_def, normalLayer_def;
  FILE *layerListFile=NULL;
  FILE *layerFile;
  float logMod=-1;
  int map[16];
  int greys[16];
  int nonBlack;
  float map_v;
  int currentLayerAddress, nextLayerAddress=0, layerDataAddress;
  pid_t myPid = getpid();
  while ((o=getopt(argc, argv, "f:P:L:s:e:o:I:N:l:F:M:W:GX:B:T:"))!=-1) {
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
	startExp = atof(optarg);
	break;
      case 'e':
	endExp = atof(optarg);
	break;
      case 'o':
	offset = strtol(optarg, NULL, 0);
	break;
      case 'I':
	individualParams = strtol(optarg, NULL, 0);
	setIndividualParams = 1;
	break;
      case 'N':
	normalExposureTime = atof(optarg);
	break;
      case 'B':
	bottomExposureTime = atof(optarg);
	break;
      case 'T':
	layerThickness = atof(optarg);
	break;
      case 'F':
	flags = strtol(optarg, NULL, 0);
	break;
      case 'l':
	if (!(layerListFile = fopen(optarg,"r"))) {
	  perror(optarg);
	  return -1;
	}
	break;
      case 'M':
	logMod = atof(optarg);
	break;
      case 'W':
	workDirName = optarg;
	while (strlen(workDirName) > 1 && workDirName[strlen(workDirName) - 1] == '/') {
	  workDirName[strlen(workDirName) - 1] = '\0';
	}
	if (stat(workDirName, &workDirStat) != 0) {
	  perror(workDirName);
	  return -2;
	}
	if (!(workDirStat.st_mode & S_IFDIR)) {
	  fprintf(stderr, "%s: not a directory.\n", workDirName);
	  return -2;
	}
	break;
      case 'G':
	countGreys = 1;
	break;
      case 'X':
	exposureSteps = strtol(optarg, NULL, 0);
	break;
      default:
	usage(argv[0]);
	return -1;
	break;
      }
  }
  if (!photonsFile || (logMod > -1 && workDirName==NULL) || (startExp * endExp < 0) || (exposureSteps > 0 && workDirName == NULL)) {
    usage(argv[0]);
    return -1;
  }

  if (flags & FLAG_BITMAPS_AS_PNG) {
    layerSuffix="png";
  }

  memset(greys, 0, 16 * sizeof(int));

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
  if (pfh.unknown1Address != 0) {
    fseek(photonsFile, pfh.unknown1Address, SEEK_SET);
    if (fread(&u1h, sizeof(u1h), 1, photonsFile) != 1) {
      fprintf(stderr, "Short read on unknown1 header.\n");
      return -10;
    }
  }
  fseek(photonsFile, pfh.layerDefAddress, SEEK_SET);
  if (fread(&ldh, sizeof(ldh), 1, photonsFile) != 1) {
    fprintf(stderr, "Short read on layerdefs header.\n");
    return -10;
  }

  // ****** Print info
  printf("********* File header *********\n");
  printFileHeader(stdout, &pfh);
  printf("********* Photons header *********\n");
  printHeader(stdout, &ph);
  printf("********* Preview header *********\n");
  printPreviewHeader(stdout, &pvh);
  if (pfh.unknown1Address != 0) {
    printf("********* Unknown area 1 *********\n");
    printUnknown1Header(stdout, &u1h);
  }
  printf("********* LayerDefs header *********\n");
  printLayerDefHeader(stdout, &ldh);
  if (countGreys) {
    printf("********* Gray values count *********\n");
    for (i=0; i<ldh.nlayers; i++) {
      fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh) + i * sizeof(ldl), SEEK_SET);
      if (fread(&ldl, sizeof(ldl), 1, photonsFile) != 1) {
	fprintf(stderr, "Short read on layerdefs layer header.\n");
	return -10;
      }
      fseek(photonsFile, ldl.address, SEEK_SET);
      if (fread(encodedBuf, 1, ldl.datalen, photonsFile) != ldl.datalen) {
	fprintf(stderr, "Short read on layer image.\n");
	return -10;
      }
      rawImageSize = rleDecode(encodedBuf, ldl.datalen, rawBuf, sizeof(rawBuf));
      for (int j = 0; j < rawImageSize; j++) {
	greys[(rawBuf[j] & 0xf0) >> 4]++;
	greys[rawBuf[j] & 0x0f]++;
      }
    }
    for (i=0; i<16; i++) {
      printf("Grey %1X: %d\n", i, greys[i]);
    }
  }
  printf("********* Layers *********\n");
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
      fseek(photonsFile, ldl.address, SEEK_SET);
      if (fread(encodedBuf, 1, ldl.datalen, photonsFile) != ldl.datalen) {
	fprintf(stderr, "Short read on layer image.\n");
	return -10;
      }
      sprintf(filenamebuf, "%s_%04d.%s", layerPrefix, i, layerSuffix);
      if (!(outfile = fopen(filenamebuf, "w"))) {
	perror(filenamebuf);
	return -2;
      }
      if (flags & FLAG_DO_NOT_DECODE_LAYERS) {
	fwrite(encodedBuf, 1, ldl.datalen, outfile);
	printf("Extracted encoded layer %d as %s, size %d\n", i, filenamebuf, ldl.datalen);
      } else if (flags & FLAG_BITMAPS_AS_PNG) {
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	size_t y;
	png_byte ** row_pointers = NULL;
	rawImageSize = rleDecode(encodedBuf, ldl.datalen, rawBuf, sizeof(rawBuf));
	png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
	  fprintf(stderr, "png_create_write_struct() failed\n");
	  return -21;
	}

	info_ptr = png_create_info_struct (png_ptr);
	if (info_ptr == NULL) {
	  fprintf(stderr, "png_create_info_struct() failed\n");
	  return -21;
	}

	/* Set up error handling. */

	if (setjmp (png_jmpbuf (png_ptr))) {
	  fprintf(stderr, "General PNG creation failure.\n");
	  return -21;
	}

	/* Set image attributes. */

	png_set_IHDR (png_ptr,
		      info_ptr,
		      ph.resX,
		      ph.resY,
		      4,
		      PNG_COLOR_TYPE_GRAY,
		      PNG_INTERLACE_NONE,
		      PNG_COMPRESSION_TYPE_DEFAULT,
		      PNG_FILTER_TYPE_DEFAULT);

	/* Initialize rows of PNG. */

	row_pointers = png_malloc (png_ptr, ph.resY * sizeof (png_byte *));
	for (y = 0; y < ph.resY; y++) {
	  png_byte *row = png_malloc (png_ptr, sizeof (uint8_t) * ph.resX / 2);
	  row_pointers[y] = row;
	  memcpy(row_pointers[y], rawBuf + y * ph.resX / 2, ph.resX / 2);
	}

	/* Write the image data to "fp". */

	png_init_io (png_ptr, outfile);
	png_set_rows (png_ptr, info_ptr, row_pointers);
	png_write_png (png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

	for (y = 0; y < ph.resY; y++) {
	  png_free (png_ptr, row_pointers[y]);
	}
	png_free (png_ptr, row_pointers);
	png_destroy_write_struct(&png_ptr, &info_ptr);
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

  // Set the bottom exposure
  if (bottomExposureTime > -1) {
    ph.bottomExposureTime = bottomExposureTime;
    headerChanged=1;
  }

  // Set the bottom exposure
  if (layerThickness > -1) {
    ph.zThickness = layerThickness;
    headerChanged=1;
  }

  // Write out the modified header
  if (headerChanged) {
    fseek(photonsFile, pfh.headerAddress, SEEK_SET);
    fwrite(&ph, sizeof(ph), 1, photonsFile);
  }

  // Set layer thickness
  if (layerThickness > 0) {
    for (i=0; i<ldh.nlayers; i++) {
      fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh) + i * sizeof(ldl), SEEK_SET);
      if (fread(&ldl, sizeof(ldl), 1, photonsFile) != 1) {
	fprintf(stderr, "Short read on layerdefs layer header.\n");
	return -10;
      }
      
      ldl.layerThickness = layerThickness;
      fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh) + i * sizeof(ldl), SEEK_SET);
      fwrite(&ldl, sizeof(ldl), 1, photonsFile);
    }
  }

    // Set individual exposure times on layers
  if (exposureSteps <= 0 && startExp > 0 && endExp > 0) {
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

  // Set individual exposure times on individual slices for all layers after <offset>
  // We will need to use a work directory for this, to generate many more layers than
  // the file originally contained.
  if (exposureSteps > 0 && startExp > 0) {
    for (currentLayer = 0; currentLayer < ldh.nlayers; currentLayer++) {
      fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh) + currentLayer * sizeof(ldl), SEEK_SET);
      if (fread(&ldl, sizeof(ldl), 1, photonsFile) != 1) {
	fprintf(stderr, "Short read on layerdefs layer header.\n");
	return -10;
      }

      sprintf(filenamebuf, "%s/layer%d_%04d.bin", workDirName, myPid, currentLayer);
      if (!(outfile = fopen(filenamebuf, "w"))) {
	perror(filenamebuf);
	return -2;
      }
      fseek(photonsFile, ldl.address, SEEK_SET);
      if (fread(encodedBuf, 1, ldl.datalen, photonsFile) != ldl.datalen) {
	fprintf(stderr, "Short read on layer image.\n");
	return -10;
      }
      rawImageSize = rleDecode(encodedBuf, ldl.datalen, rawBuf, sizeof(rawBuf));
      fwrite(rawBuf, 1, rawImageSize, outfile);
      printf("Extracted layer %d as %s, size %d\n", currentLayer, filenamebuf, rawImageSize);
      fclose(outfile);
    }
    nextLayerAddress = pfh.layerDefAddress + sizeof(ldh) + (ldh.nlayers + (ldh.nlayers - (ph.bottomLayers + offset)) * (exposureSteps-1)) * sizeof(ldl);

    // We enter the loop with the file positioned at 'nextLayerAddress'
    fseek(photonsFile, nextLayerAddress, SEEK_SET);
    int actualLayer = 0;
    for (currentLayer=0; currentLayer<ldh.nlayers; currentLayer++) {

      // Read layer data from the layer file created in the previous step
      sprintf(filenamebuf, "%s/layer%d_%04d.bin", workDirName, myPid, currentLayer);
      if (!(layerFile = fopen(filenamebuf, "r"))) {
	perror(filenamebuf);
	return -3;
      }
      rawImageSize=fread(rawBuf, 1, sizeof(rawBuf), layerFile);
      fclose(layerFile);
      
      if (currentLayer >= ph.bottomLayers + offset) {
	for (i = 0; i < exposureSteps; i++) {
	  // encode and save layer, save the address to the next layer
	  // then mask off a segment of it and loop
	  // First layer gets the proper layer thickness and exposure = startExp,
	  // subsequent ones get layer thickness 0 and exposure = (endExp - startExp)/exposureSteps
	  
	  // Save address in currentLayerAddress
	  currentLayerAddress = nextLayerAddress;

	  nonBlack=0;
	  for (int j=0; j<rawImageSize; j++) {
	    if (rawBuf[j] & 0xf0) {
	      nonBlack++;
	    }
	    if (rawBuf[j] & 0x0f) {
	      nonBlack++;
	    }
	  }
	  
	  // Do the run-length encoding
	  encodedImageSize = rleEncode(rawBuf, rawImageSize, encodedBuf, sizeof(encodedBuf));
	  
	  // Write the encoded data to the photons file at the current position
	  fwrite(encodedBuf, 1, encodedImageSize, photonsFile);
	  printf("Added %s sublayer %d to file, encoded size %d.\n", filenamebuf, i, encodedImageSize);
	  
	  nextLayerAddress = currentLayerAddress + encodedImageSize;
	  
	  // Set address and datalen
	  // We're assuming we have a correct header in ldl here, but we are
	  // filling out most of it anyway
	  ldl.address = currentLayerAddress;
	  ldl.datalen = encodedImageSize;
	  ldl.nonBlackPixels = nonBlack;
	  if (i == 0) {
	    ldl.expTime = startExp;
	  } else {
	    ldl.expTime = (endExp - startExp) / (float)exposureSteps;
	  }

	  if (i == exposureSteps - 1) {
	    ldl.layerThickness = ph.zThickness;
	    ldl.zHeight = 5;
	  } else {
	    ldl.layerThickness = 0;
	    ldl.zHeight = 0.1;
	  }
	  // write LayersDef - we need to use the actual layer number here!
	  fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh) + actualLayer * sizeof(ldl), SEEK_SET);
	  fwrite(&ldl, sizeof(ldl), 1, photonsFile);
	  // seek to nextLayerAddress
	  fseek(photonsFile, nextLayerAddress, SEEK_SET);
	  for (int y = 0; y < ph.resY; y++) {
	    memset(rawBuf + (y * ph.resX + i * ph.resX / exposureSteps) / 2, 0, (ph.resX / exposureSteps) / 2);
	  }
	  actualLayer++;
	}
      } else {
	// Save address in currentLayerAddress
	currentLayerAddress = nextLayerAddress;

	nonBlack=0;
	for (int j=0; j<rawImageSize; j++) {
	  if (rawBuf[j] & 0xf0) {
	    nonBlack++;
	  }
	  if (rawBuf[j] & 0x0f) {
	    nonBlack++;
	  }
	}
	
	// Do the run-length encoding
	encodedImageSize = rleEncode(rawBuf, rawImageSize, encodedBuf, sizeof(encodedBuf));
	
	// Write the encoded data to the photons file at the current position
	fwrite(encodedBuf, 1, encodedImageSize, photonsFile);
	printf("Added %s to file, encoded size %d.\n", filenamebuf, encodedImageSize);
	// Save new address in nextLayerAddress
	nextLayerAddress = currentLayerAddress + encodedImageSize;
	
	// Bit of a shortcut here: We blindly assume that we will have a valid ldl after the first
	// few layers, and we do not try to read the original one. For these first few layers, we do
	// read the original ldl before modding it
	// Seek to correct LayersDef
	fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh) + currentLayer * sizeof(ldl), SEEK_SET);
	//    read LayersDef
	if (fread(&ldl, sizeof(ldl), 1, photonsFile) != 1) {
	  fprintf(stderr, "Short read on layerdefs layer header.\n");
	  return -10;
	}
	ldl.address = currentLayerAddress;
	ldl.datalen = encodedImageSize;
	ldl.nonBlackPixels = nonBlack;
	if (currentLayer >= ph.bottomLayers) {
	  ldl.expTime = ph.normalExposureTime;
	} else {
	  ldl.expTime = ph.bottomExposureTime;
	}
	//    write LayersDef
	fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh) + currentLayer * sizeof(ldl), SEEK_SET);
	fwrite(&ldl, sizeof(ldl), 1, photonsFile);
	//    seek to nextLayerAddress
	fseek(photonsFile, nextLayerAddress, SEEK_SET);
	actualLayer++;
      }
      if (unlink(filenamebuf) != 0) {
	perror(filenamebuf);
	return -12;
      }
    }
    // Truncate file to <nextLayer>
    fflush(photonsFile);
    if (ftruncate(fileno(photonsFile), nextLayerAddress) != 0) {
      perror ("ftruncate()");
      return -11;
    }
    ldh.nlayers += (ldh.nlayers - (ph.bottomLayers + offset)) * (exposureSteps - 1);
    // Seek to LayersDefHeader
    fseek(photonsFile, pfh.layerDefAddress, SEEK_SET);
    // Write LayersDefHeader
    fwrite(&ldh, sizeof(ldh), 1, photonsFile);
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
      if (flags && FLAG_BITMAPS_AS_PNG) {
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	png_init_io(png_ptr, layerFile);
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
	
	png_uint_32 width, height;
	int bit_depth, color_type, interlace_method, compression_method, filter_method;
	size_t y;
	
	png_get_IHDR (png_ptr,
		      info_ptr,
		      &width,
		      &height,
		      &bit_depth,
		      &color_type,
		      &interlace_method,
		      &compression_method,
		      &filter_method);
	
	if (width != ph.resX || height != ph.resY || bit_depth != 4 || color_type != PNG_COLOR_TYPE_GRAY) {
	  fprintf(stderr, "%s: wrong image format\n", filenamebuf);
	  return -22;
	}
	
	png_byte ** row_pointers = png_get_rows(png_ptr, info_ptr);
	
	for (y = 0; y < ph.resY; y++) {
	  memcpy(rawBuf + y * ph.resX / 2, row_pointers[y], ph.resX / 2);
	  png_free (png_ptr, row_pointers[y]);
	}
	png_free (png_ptr, row_pointers);
	rawImageSize = width * height/2;
	//	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      } else {
	//    RLE encode into photons file
	//    Save length in encodedImageSize
	rawImageSize=fread(rawBuf, 1, sizeof(rawBuf), layerFile);
      }
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
    
  // Apply a mod to the layer data gray values
  // Note:We're skipping the bottom layers here!
  if (logMod > 0) {
    map_v = 15 / (logf(logMod * 15) / logf(15));
    map[0] = 0;
    for (i=1; i<16; i++) {
      map[i] = (int)(0.5 + map_v * logf(logMod * (float)i) / logf(15));
      printf("Map %d to %d\n", i, map[i]);
    }
    nextLayerAddress=0;

    for (currentLayer=ph.bottomLayers; currentLayer<ldh.nlayers; currentLayer++) {
      // This is a near copy of the layer export code. Feel free to figure out an
      // elegant way to refactor both into one. I can't be bothered.
      fseek(photonsFile, pfh.layerDefAddress + sizeof(ldh) + currentLayer * sizeof(ldl), SEEK_SET);
      if (fread(&ldl, sizeof(ldl), 1, photonsFile) != 1) {
	fprintf(stderr, "Short read on layerdefs layer header.\n");
	return -10;
      }
      // Save the address of the first layer to mod, for the next loop
      if (currentLayer == ph.bottomLayers) {
	nextLayerAddress = ldl.address;
      }
      sprintf(filenamebuf, "%s/layer%d_%04d.bin", workDirName, myPid, currentLayer);
      if (!(outfile = fopen(filenamebuf, "w"))) {
	perror(filenamebuf);
	return -2;
      }
      fseek(photonsFile, ldl.address, SEEK_SET);
      if (fread(encodedBuf, 1, ldl.datalen, photonsFile) != ldl.datalen) {
	fprintf(stderr, "Short read on layer image.\n");
	return -10;
      }
      rawImageSize = rleDecode(encodedBuf, ldl.datalen, rawBuf, sizeof(rawBuf));
      fwrite(rawBuf, 1, rawImageSize, outfile);
      printf("Extracted layer %d as %s, size %d\n", currentLayer, filenamebuf, rawImageSize);
      fclose(outfile);
    }
    if (nextLayerAddress == 0) {
      fprintf(stderr, "Something went wrong when extracting layer images.\n");
      return -3;
    }
    // We enter the loop with the file positioned at 'nextLayerAddress'
    fseek(photonsFile, nextLayerAddress, SEEK_SET);
    for (currentLayer=ph.bottomLayers; currentLayer<ldh.nlayers; currentLayer++) {

      // Save address in currentLayerAddress
      currentLayerAddress = nextLayerAddress;

      // Read layer data from the layer file created in the previous step
      sprintf(filenamebuf, "%s/layer%d_%04d.bin", workDirName, myPid, currentLayer);
      if (!(layerFile = fopen(filenamebuf, "r"))) {
	perror(filenamebuf);
	return -3;
      }
      rawImageSize=fread(rawBuf, 1, sizeof(rawBuf), layerFile);
      fclose(layerFile);
      
      // Perform the transformation
      for (int i=0; i<rawImageSize; i++) {
	rawBuf[i] = (map[(rawBuf[i] & 0xF0) >> 4] << 4) | (map[rawBuf[i] & 0x0F]);
      }

      // Do the run-length encoding
      encodedImageSize = rleEncode(rawBuf, rawImageSize, encodedBuf, sizeof(encodedBuf));

      // Write the encoded data to the photons file at the current position
      fwrite(encodedBuf, 1, encodedImageSize, photonsFile);
      printf("Added %s to file, encoded size %d.\n", filenamebuf, encodedImageSize);
      //    Save new address in nextLayerAddress
      nextLayerAddress = currentLayerAddress + encodedImageSize;
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
      if (unlink(filenamebuf) != 0) {
	perror(filenamebuf);
	return -12;
      }
    }
    // Truncate file to <nextLayer>
    fflush(photonsFile);
    if (ftruncate(fileno(photonsFile), nextLayerAddress) != 0) {
      perror ("ftruncate()");
      return -11;
    }
  }
  fclose(photonsFile);
  return 0;
}
