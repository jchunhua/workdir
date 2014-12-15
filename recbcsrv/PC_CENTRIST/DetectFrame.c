#include "DetectFrame.h"

IMAGE Object;

// raw file
void DetectFrame(unsigned char* buffer, int frame_num)
{
	IMAGE *object = &Object;

	if(frame_num == 0)
		InitImage(object);

	FrameCentrist(buffer, object);
	FrameDiff(buffer, object);
	object->frame_num++;
	
	int i = 0;
	rect result = {0,0,0,0};
	rect maxRect = {INT_MAX, 0, INT_MIN, FRAME_HEIGHT};

//	show the result source.
//	0 - no object
//	1 - centrist object.
//	2 - moving object.
//	3 - both detected object.
	int result_flag = 0;

	if(object->recNum == 0 && object->cenRecNum != 0)
	{
		for(i = 0; i < object->cenRecNum; i++)
		{
			rect item = object->cenCoord[i];
			result.x1 += item.x1;
			result.y1 += item.y1;
			result.x2 += item.x2;
			result.y2 += item.y2;
			if(maxRect.x1 > item.x1) maxRect.x1 = item.x1;
			if(maxRect.x2 < item.x2) maxRect.x2 = item.x2;
		}
		result.x1 = result.x1/object->cenRecNum;
		result.y1 = result.y1/object->cenRecNum;
		result.x2 = result.x2/object->cenRecNum;
		result.y2 = result.y2/object->cenRecNum;
		//centrist object.
		result_flag = 1;
	}else if(object->recNum != 0)
	{
		int left = object->coordinate[0].x1;
		int right = object->coordinate[object->recNum-1].x2;

		int num = 0;
		for(i = 0; i < object->cenRecNum; i++)
		{
			rect item = object->cenCoord[i];
			if(item.x1 > (left - DETECT_GAP) && item.x2 < (right + DETECT_GAP))
			{
			 	result.x1 += item.x1;
				result.y1 += item.y1;
				result.x2 += item.x2;
				result.y2 += item.y2;
				num++;
				
				if(maxRect.x1 > item.x1) maxRect.x1 = item.x1;
				if(maxRect.x2 < item.x2) maxRect.x2 = item.x2;
			}
		}

		if(num > 0)
		{
			result.x1 = result.x1/num;
			result.x2 = result.x2/num;
			result.y1 = result.y1/num;
			result.y2 = result.y2/num;
			//centrist and moving detect.
			result_flag = 3;
		}else
		{
			result.x1 = object->coordinate[0].x1;
			result.x2 = object->coordinate[object->recNum-1].x2;
			result.y1 = 0;
			result.y2 = FRAME_HEIGHT;

			if(maxRect.x1 > result.x1) maxRect.x1 = result.x1;
			if(maxRect.x2 < result.x2) maxRect.x2 = result.x2;
			//moving object.
			result_flag = 2;
		}
	}

	// top left bottom right
	int *iPtr = (int *)buffer;
	if(result.x2 != 0 && result.y2 != 0)
		iPtr[0] = result_flag;
	else
		iPtr[0] = 0;

	iPtr[1] = result.y1;
	iPtr[2] = result.x1;
	iPtr[3] = result.y2;
	iPtr[4] = result.x2;

	iPtr[5] = maxRect.y1;
	iPtr[6] = maxRect.x1;
	iPtr[7] = maxRect.y2;
	iPtr[8] = maxRect.x2;
}
