#include "CtImage.h"
#include "svm.h"
#include "param.h"

const int BHEIGHT = PHEIGHT/9;
const int BWIDTH = PWIDTH/4;
const int SBNUM_H = 8;
const int SBNUM_W = 3;

extern svm_model* model;

double* scaleFeature;
int* feature;
short * testSV;

extern int *Feature_Min;
extern int *Feature_Max;

void ComputeSobel(uchar* image, uchar* sobelImage, int height, int width)
{
	int i = 0, j = 0;
	uchar value = 0;
	uchar *p = image;
	uchar *pr = sobelImage;

    for( i=1;i< height-1;i++)
    {
		uchar* p1 = &p[(i-1)*width];
		uchar* p2 = &p[i*width];
		uchar* p3 = &p[(i+1)*width];
        for( j=1;j< width-1;j++)
        {
            int gx =     p1[j-1] - p1[j+1]
                     + 2*(p2[j-1]   - p2[j+1])
                     +    p3[j-1] - p3[j+1];

            int gy =     p1[j-1] - p3[j-1]
                     + 2*(p1[j]   - p3[j])
                     +    p1[j+1] - p3[j+1];

           value = (uchar)sqrt((float)(gx*gx+gy*gy));
	       pr[i*width+j] =  value * (value > THRESH);
        }
    }
}

void ComputeCt(uchar* image, uchar* ctImage, int height, int width)
{
	int i = 0, j = 0;
	uchar* ctp = ctImage;
	uchar* p = image;
	for( i=2;i<height-2;i++)
	{
		uchar* p1 = p+(i-1)*width;
		uchar* p2 = p+i*width;
		uchar* p3 = p+(i+1)*width;

		for( j=2;j<width-2;j++)
		{
			int index = 0;
			if(p2[j]<=p1[j-1]) index += 0x80;
			if(p2[j]<=p1[j]) index += 0x40;
			if(p2[j]<=p1[j+1]) index += 0x20;
			if(p2[j]<=p2[j-1]) index += 0x10;
			if(p2[j]<=p2[j+1]) index += 0x08;
			if(p2[j]<=p3[j-1]) index += 0x04;
			if(p2[j]<=p3[j]) index += 0x02;
			if(p2[j]<=p3[j+1]) index ++;
			ctp[i*width+j] = index;
		}
	}
}

void GetFeatureVector(uchar* ctImage, int t, int l, int width)
{
	double low = -1.0, up = 1.0;
    int i;
    int tmp;
    
	memset(feature,0, DIM*sizeof(int));

	uchar* ctp = ctImage;
	int* wp = feature;
	int xdiv = 0, ydiv = 0, x = 0, y = 0;
	for ( xdiv = 0; xdiv<SBNUM_H; xdiv++)
		for( ydiv = 0; ydiv<SBNUM_W; ydiv++)
		{
			for ( x = 1; x < BHEIGHT*2-1; ++x)
			{
				for ( y = 1; y < BWIDTH*2-1; ++y)
				{
					int index = (xdiv*SBNUM_W+ydiv)*256 + ctp[(t+xdiv*BHEIGHT+x)*width+(l+ydiv*BWIDTH+y)];
					wp[index]++;

				}
			}
		}

	for ( i = 0; i < DIM; i++) //scale
	{
		if (feature[i] == Feature_Min[i])
			scaleFeature[i] = low;
		else if (feature[i] == Feature_Max[i])
			scaleFeature[i] = up;
		else
			scaleFeature[i] = low
					+ (up - low) * (double)(feature[i] - Feature_Min[i])
							/ (Feature_Max[i] - Feature_Min[i]);
        tmp = scaleFeature[i] * SCALE;
        testSV[i] = tmp >= 128 ? 127:tmp;
	}
}

void FrameCentrist(unsigned char *nBuffer, IMAGE *object)
{
	object->cenRecNum = 0;

	uchar* sobelImage = (uchar *)malloc(FRAME_WIDTH*FRAME_HEIGHT*sizeof(uchar));
	uchar* ctImage = (uchar *)malloc(FRAME_WIDTH*FRAME_HEIGHT*sizeof(uchar));

	ComputeSobel((uchar *)nBuffer, sobelImage, FRAME_HEIGHT, FRAME_WIDTH);
	ComputeCt(sobelImage, ctImage, FRAME_HEIGHT, FRAME_WIDTH);

	feature = (int*) malloc(DIM*sizeof(int));
	scaleFeature = (double *)malloc(sizeof(double)*DIM);
	testSV = (short*)malloc(sizeof(short)*DIM);

	int x = 0, y = 0;
	int stepsizex = FRAME_HEIGHT/12;
	int stepsizey =	FRAME_WIDTH/12;

	for ( x = 2; x <= FRAME_HEIGHT - PHEIGHT-2; x+=stepsizex)
	{
		for ( y = 2; y <= FRAME_WIDTH - PWIDTH-2; y+=stepsizey)
		{
			GetFeatureVector(ctImage,x,y,FRAME_WIDTH);

			float s = svm_predict(model, testSV);
			if (s != 0)
			{
				object->cenCoord[object->cenRecNum].x1 = y;
				object->cenCoord[object->cenRecNum].y1 = x;
				object->cenCoord[object->cenRecNum].x2 = y + PWIDTH;
				object->cenCoord[object->cenRecNum].y2 = x + PHEIGHT;
				object->cenRecNum++;
			}
		}
	}

	free(scaleFeature);
	free(feature);
	free(testSV);
	free(sobelImage);
	free(ctImage);
}
