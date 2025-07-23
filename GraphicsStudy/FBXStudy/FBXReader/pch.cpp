// pch.cpp: source file corresponding to the pre-compiled header

#include "pch.h"

// When you are using pre-compiled headers, this source file is necessary for compilation to succeed.
HeapLumps fmhl;

thread_local unsigned int threadID;

FM_System0* fm;

RangeArr<si32, pair<si32, ui32>> UnReleasedAlternateAddressRangeArr;

//unsigned char datamem[4096*16];
//int dataUp = 0;

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

//void InitDA() {
//	for (int i = 0; i < 4096 * 16; ++i) {
//		datamem[i] = i & 255;
//	}
//}
//
//void* DAlloc(int size) {
//	void* ptr = &datamem[dataUp];
//	dataUp += size + 8;
//	return ptr;
//}
//
//void outFAlloc() {
//	ofstream ofs{ "DAlloc_Meta" };
//	int cnt = 0;
//	for (int i = 0; i < dataUp; ++i) {
//		if (datamem[i] == (unsigned char)(i & 255)) {
//			ofs << '0';
//		}
//		else {
//			ofs << '1';
//		}
//		cnt += 1;
//		if (cnt >= 32) {
//			cnt = 0;
//			ofs << endl;
//		}
//	}
//}