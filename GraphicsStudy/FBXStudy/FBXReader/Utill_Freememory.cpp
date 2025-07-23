#include "Utill_FreeMemory.h"

HeapLumps fmhl;

thread_local unsigned int threadID;

FM_System0* fm;

int getcost(int n, int size)
{
	ui32 k = !!(n % size);
	ui32 dd = (n / size) + k;
	ui32 r = (8 * size) * dd + dd;
	return r;
}

int minarr(int siz, int* arr, int* indexout)
{
	int min = arr[0];
	for (int i = 0; i < siz; ++i)
	{
		if (min > arr[i])
		{
			min = arr[i];
			*indexout = i;
		}
	}
	return min;
}