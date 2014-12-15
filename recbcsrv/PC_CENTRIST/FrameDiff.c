#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include "CtImage.h"

const int  VERTICAL_THRESH = 20;
const int  PIXEL_DIFF_THRESH = 20;
const int  OBJECT_SIZE = 5;
const int  DETECT_GAP = PWIDTH*2/3;

void InitImage(IMAGE *object)
{
	object->has_object = false;
	object->recNum = 0;
	object->cenRecNum = 0;
	object->frame_num = 0;
}

void FrameDiff(unsigned char *Y_value, IMAGE* object)
{
	if(object->frame_num == 0)
	{
		memcpy(object->firstImage, Y_value, FRAME_WIDTH*FRAME_HEIGHT);
	}
	object->recNum = 0;              

	int i,j;

	if(object->frame_num >= 1)
	{
		for( i=0;i<FRAME_HEIGHT;i++)
		{
			for( j=0;j<FRAME_WIDTH;++j)
			{
				if(abs(Y_value[i*FRAME_WIDTH+j] - object->lastImage[i*FRAME_WIDTH+j]) > PIXEL_DIFF_THRESH)
					object->diffImage[i*FRAME_WIDTH+j] = 255;
				else object->diffImage[i*FRAME_WIDTH+j] = 0;
			}
		}
   
		// vertical computation.
		memset(object->vertical,0,FRAME_WIDTH*sizeof(int));

		int flagVer=0,temp=0;

		for( j=0;j<FRAME_WIDTH;j++)
		{
			for( i=0;i<FRAME_HEIGHT;i++)
			{
				object->vertical[j]+=object->diffImage[j+i*FRAME_WIDTH]/255;
			}

			if(object->vertical[j]>VERTICAL_THRESH && flagVer==0)
			{
				temp=j;
				flagVer=1;
			}
	
			if((object->vertical[j]<=VERTICAL_THRESH && flagVer==1 && j<FRAME_WIDTH-1)||(j==FRAME_WIDTH-1 && flagVer==1))
			{
				if((j-temp)>OBJECT_SIZE && object->recNum < COOR_SIZE-4)
				{
					object->coordinate[object->recNum].x1 = temp;
					object->coordinate[object->recNum].x2 = j;
					object->coordinate[object->recNum].y1 = 0;
					object->coordinate[object->recNum].y2 = FRAME_HEIGHT;
					object->recNum++;
				}
				flagVer=0;
			}
		}

	}

	memcpy(object->lastImage, Y_value, FRAME_WIDTH*FRAME_HEIGHT);
}