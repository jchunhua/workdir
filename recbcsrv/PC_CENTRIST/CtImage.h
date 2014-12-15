#ifndef _CTIMAGE_H_
#define _CTIMAGE_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "param.h"

typedef unsigned char uchar;

typedef struct rect
{
	int x1;
	int y1;
	int x2;
	int y2;
}rect;

typedef struct
{
	int frame_num;
	int recNum;
	int cenRecNum;
	unsigned char firstImage[FRAME_WIDTH*FRAME_HEIGHT];
	unsigned char lastImage[FRAME_WIDTH*FRAME_HEIGHT];
	unsigned char filterImage[FRAME_WIDTH*FRAME_HEIGHT];
	unsigned char diffImage[FRAME_WIDTH*FRAME_HEIGHT];
	int horizontal[FRAME_HEIGHT];
	int vertical[FRAME_WIDTH];
	rect coordinate[COOR_SIZE];
	rect cenCoord[COOR_SIZE];
	int has_object;
} IMAGE;

extern const int BHEIGHT;
extern const int BWIDTH;
extern const int SBNUM_H;
extern const int SBNUM_W;

extern const int  VERTICAL_THRESH;
extern const int  PIXEL_DIFF_THRESH;
extern const int  OBJECT_SIZE;
extern const int  DETECT_GAP;


extern void ComputeSobel(uchar* image, uchar* sobelImage, int height, int width);
extern void ComputeCt(uchar* image, uchar* ctImage, int height, int width);

extern void GetFeatureVector(uchar* ctImage, int t, int l, int width);
extern void FrameCentrist(unsigned char *nBuffer, IMAGE* object);

extern void InitImage(IMAGE *object);
extern void VerticalLine(IMAGE *object);
extern void FrameDiff(unsigned char *Y_value, IMAGE* object);

extern IMAGE object;
#endif
