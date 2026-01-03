#pragma once
#include <Windows.h>
#include <intrin.h>

typedef unsigned char ui8;
typedef unsigned short ui16;
typedef unsigned int ui32;
typedef unsigned long long ui64;

/*
* 설명 : vector와 구조가 유사하지만, 비트플레그로 해당 공간의 할당/해제를 파악할 수 있고,
* 이를 통해 재활용이 가능한 자료구조.
*/
template <typename T> struct vecset {
	T* Arr; // [Data Arr] [ mask[Capacity / 8 byte], ceil to sizeof(T)]
	int Capacity;
	int size;
	int endof_maxFlagarr = 0;
	ui32 extraData;

	__forceinline ui32* getAllocFlag() {
		return (ui32*)&Arr[Capacity];
	}

	__declspec(property(get = getAllocFlag)) ui32* AllocFlag;

	void Init(int capacity) {
		int n = (capacity / 8);
		int k = n / sizeof(T) + (n % sizeof(T) != 0) ? 1 : 0;

		int AllocSiz = k + capacity;
		T* newArr = new T[AllocSiz];
		if (Arr != nullptr) {
			ZeroMemory(newArr, sizeof(T) * AllocSiz);

			int temp = (Capacity / 8);
			int AdditionSiz = n / sizeof(T) + (n % sizeof(T) != 0) ? 1 : 0;

			memcpy_s(&newArr[capacity], AdditionSiz * sizeof(T), &Arr[Capacity], AdditionSiz * sizeof(T));
			memcpy_s(newArr, Capacity * sizeof(T), Arr, Capacity * sizeof(T));
			delete[] Arr;
		}
		else {
			size = 0;
			ZeroMemory(newArr, sizeof(T) * AllocSiz);
		}
		Arr = newArr;
		Capacity = capacity;
	}

	int GetRecyleIndex() {
		constexpr ui32 maxFlag = 0xFFFFFFFF;
		int m = Capacity >> 5;
		ui32* flagArr = AllocFlag;
		int r = 0;
		for (int i = endof_maxFlagarr; i < m; ++i) {
			if (flagArr[i] != maxFlag) {
				int x = flagArr[i];
				x = ~x;
				x = (x & -x);
				x = (!x) ? 0xF0000000 : _tzcnt_u32(x);
				r = (i << 5) + x;
				goto CHECK_IN_SIZE;
			}
			else {
				endof_maxFlagarr += 1;
			}
		}

	CHECK_IN_SIZE:
		if (r < size) return r;
		return -1;
	}

	int Alloc() {
		int index = GetRecyleIndex();
		if (index >= 0) {
			goto ALLOC_CODE;
		}
		else {
			if (size >= Capacity) {
				Init(Capacity << 1);
			}
			index = size;
			size += 1;
			goto ALLOC_CODE;
		}

	ALLOC_CODE:
		int pi = index >> 5;
		int ci = index & 31;
		ui32& flag = AllocFlag[pi];
		flag |= (1 << ci);
		return index;
	}

	void Free(int index) {
		int pi = index >> 5;
		int ci = index & 31;
		ui32& flag = AllocFlag[pi];
		flag &= ~(1 << ci);
		if (pi <= endof_maxFlagarr) endof_maxFlagarr = pi;
		if (index == size - 1) size -= 1;
	}

	__forceinline T& operator[](int index) { return Arr[index]; }
	__forceinline T& at(int index) { if (index < size) return Arr[index]; return nullptr; }
	__forceinline bool isnull(int index) {
		int pi = index >> 5; // SHR (R32, I8) lat 1	throuput 0.50 / 0.50 port 1*p06
		int ci = index & 31;
		ui32 flag = AllocFlag[pi];
		return !(flag & (1 << ci)); // SHL (R32, CL) lat[0;2] throuput 1.00 / 1.00 port 2*p06
	}
	__forceinline bool isAlloc(int index) {
		int pi = index >> 5; // SHR (R32, I8) lat 1	throuput 0.50 / 0.50 port 1*p06
		int ci = index & 31;
		ui32 flag = AllocFlag[pi];
		return (flag & (1 << ci)); // SHL (R32, CL) lat[0;2] throuput 1.00 / 1.00 port 2*p06
	}
};


/*
* // vecset without data.
* 설명 : 비트를 통해 어떤 공간의 할당여부를 감시할 수 있는 할당자 자료구조
*/
struct BitAllotter {
	ui32* AllocFlag = nullptr;
private:
	int _Capacity;
	int _size;
	int endof_maxFlagarr = 0;
	ui32 extraData; // 0 -> fix capacity, 1 -> dynamic capacity
public:
	__forceinline bool GetDynamicCapacityMode() {
		return extraData & 1;
	}
	__forceinline void SetDynamicCapacityMode(bool b) {
		if (b) {
			extraData |= 1;
		}
		else {
			extraData &= 0xFFFFFFFE;
		}
	}
	__declspec(property(get = GetDynamicCapacityMode, put = SetDynamicCapacityMode)) bool isDynamicCapacityMode;

	__forceinline const int GetCapacity() { return _Capacity; };
	__declspec(property(get = GetCapacity)) const int Capacity;

	__forceinline const int GetSize() { return _size; };
	__declspec(property(get = GetSize)) const int size;

	void Init(int capacity) {
		int AllocSiz = capacity / 32;
		ui32* newArr = new ui32[AllocSiz];
		if (AllocFlag != nullptr && extraData) {
			ZeroMemory(newArr, sizeof(ui32) * AllocSiz);

			int len = _Capacity * sizeof(ui32) / 32;
			memcpy_s(newArr, len, AllocFlag, len);
			delete[] AllocFlag;
		}
		else {
			_size = 0;
			endof_maxFlagarr = 0;
			extraData = 0;
			ZeroMemory(newArr, sizeof(ui32) * AllocSiz);
		}
		AllocFlag = newArr;
		_Capacity = capacity;
	}

	int GetRecyleIndex() {
		constexpr ui32 maxFlag = 0xFFFFFFFF;
		int m = Capacity >> 5;
		ui32* flagArr = AllocFlag;
		int r = 0;
		for (int i = endof_maxFlagarr; i < m; ++i) {
			if (flagArr[i] != maxFlag) {
				int x = flagArr[i];
				x = ~x;
				x = (x & -x);
				x = (!x) ? 0xF0000000 : _tzcnt_u32(x);
				r = (i << 5) + x;
				goto CHECK_IN_SIZE;
			}
			else {
				endof_maxFlagarr += 1;
			}
		}

	CHECK_IN_SIZE:
		if (r < size) return r;
		return -1;
	}

	int Alloc() {
		int index = GetRecyleIndex();
		if (index >= 0) {
			goto ALLOC_CODE;
		}
		else {
			if (size >= Capacity) {
				Init(Capacity << 1);
			}
			index = size;
			_size += 1;
			goto ALLOC_CODE;
		}

	ALLOC_CODE:
		int pi = index >> 5;
		int ci = index & 31;
		ui32& flag = AllocFlag[pi];
		flag |= (1 << ci);
		return index;
	}

	void Free(int index) {
		int pi = index >> 5;
		int ci = index & 31;
		ui32& flag = AllocFlag[pi];
		flag &= ~(1 << ci);
		if (pi <= endof_maxFlagarr) endof_maxFlagarr = pi;
		if (index == size - 1) _size -= 1;
	}

	__forceinline bool isnull(int index) {
		int pi = index >> 5; // SHR (R32, I8) lat 1	throuput 0.50 / 0.50 port 1*p06
		int ci = index & 31;
		ui32 flag = AllocFlag[pi];
		return !(flag & (1 << ci)); // SHL (R32, CL) lat[0;2] throuput 1.00 / 1.00 port 2*p06
	}

	__forceinline bool isAlloc(int index) {
		int pi = index >> 5; // SHR (R32, I8) lat 1	throuput 0.50 / 0.50 port 1*p06
		int ci = index & 31;
		ui32 flag = AllocFlag[pi];
		return (flag & (1 << ci)); // SHL (R32, CL) lat[0;2] throuput 1.00 / 1.00 port 2*p06
	}
};

/*
* // vecset without data.
* 설명 : Capcity와 Size를 통해 vector와 같이 동작하는 할당자 자료구조
*/
struct NextAllotter {
	int Capacity;
	int size;

	int Alloc(int scale) {
		int n = size;
		size += scale;
		if (size > Capacity) {
			return -1;
		}
		return n;
	}

	void FreeAll() {
		size = 0;
	}
};