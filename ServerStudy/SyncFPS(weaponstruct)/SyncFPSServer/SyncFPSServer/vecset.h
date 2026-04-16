#pragma once
#include <Windows.h>
#include <intrin.h>

typedef unsigned char ui8;
typedef unsigned short ui16;
typedef unsigned int ui32;
typedef unsigned long long ui64;

struct indexRange {
	int start;
	int end;
	indexRange() {}
	indexRange(int s, int e) :
		start{ s }, end{ e }
	{

	}
};

// vecset without data.
struct BitAllotter {
	ui32* AllocFlag = nullptr;
public:
	int Capacity = 0;
	int size = 0;
	int endof_maxFlagarr = 0;
	ui32 extraData = 0; // 0 -> fix capacity, 1 -> dynamic capacity

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

	void Init(int capacity) {
		int AllocSiz = capacity / 32;
		ui32* newArr = new ui32[AllocSiz];
		if (AllocFlag != nullptr && extraData) {
			ZeroMemory(newArr, sizeof(ui32) * AllocSiz);

			int len = Capacity * sizeof(ui32) / 32;
			memcpy_s(newArr, len, AllocFlag, len);
			delete[] AllocFlag;
		}
		else {
			size = 0;
			endof_maxFlagarr = 0;
			extraData = 0;
			ZeroMemory(newArr, sizeof(ui32) * AllocSiz);
		}
		AllocFlag = newArr;
		Capacity = capacity;
	}

	void Release() {
		delete[] AllocFlag;
		ZeroMemory(this, sizeof(BitAllotter));
	}

	int GetRecyleIndex() {
		constexpr ui32 maxFlag = 0xFFFFFFFF;
		int m = Capacity >> 5;
		ui32* flagArr = AllocFlag;
		int r = maxFlag >> 1;
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
		if (index >= Capacity) return;
		int pi = index >> 5;
		int ci = index & 31;
		ui32& flag = AllocFlag[pi];
		flag &= ~(1 << ci);
		if (pi <= endof_maxFlagarr) endof_maxFlagarr = pi;
		if (index == size - 1) size -= 1;
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

	__forceinline int IncNum(int index) {
		ui64 flag = 0x0000000100000000 | AllocFlag[index >> 5];//*(ui64*)&AllocFlag[index >> 5];
		ui32 t = (index + 1) & 31;
		return (!!t) * _tzcnt_u64(flag >> t) + 1;
	}
	__forceinline int InvIncNum(int index) {
		ui64 flag = /*~*(ui64*)&AllocFlag[index >> 5];*/0x0000000100000000 | ~(AllocFlag[index >> 5]);
		ui32 t = (index + 1) & 31;
		return (!!t) * _tzcnt_u64(flag >> t) + 1;
	}

	__forceinline void GetTourPairs(indexRange* out, int* outlen) {
		if (size == 0) {
			*outlen = 0;
			return;
		}
		bool enable = isAlloc(0);
		indexRange curr;
		if (enable) {
			curr.start = 0;
		}
		int up = 0;
		for (int i = 1; i < size;) {
			bool isalloc = isAlloc(i);
			if (!enable && isalloc) {
				curr.start = i;
				enable = true;
			}
			else if (enable && !isalloc) {
				curr.end = i - 1;
				out[up] = curr;
				up += 1;
				enable = false;
			}
			if (enable) {
				i += InvIncNum(i);
			}
			else {
				i += IncNum(i);
			}
		}

		if (enable) {
			curr.end = size - 1;
			out[up] = curr;
			up += 1;
			enable = false;
		}

		*outlen = up;
	}
};

template <typename T> struct vecset {
	T* Arr = nullptr;
	BitAllotter Alloter;

	vecset() {
		ZeroMemory(this, sizeof(vecset<T>));
	}

	__forceinline const int GetSize() const { return Alloter.size; }
	__declspec(property(get = GetSize)) const int size;

	__forceinline const int GetCapacity() const { return Alloter.Capacity; }
	__declspec(property(get = GetCapacity)) const int Capacity;

	__forceinline const ui32* const GetAllocFlags() const { return Alloter.AllocFlag; }
	__declspec(property(get = GetAllocFlags)) const ui32* AllocFlag;

	void Init(int capacity) {
		T* newArr = new T[capacity];

		if (Arr != nullptr) {
			memcpy_s(newArr, Alloter.Capacity * sizeof(T), Arr, Alloter.Capacity * sizeof(T));
			Alloter.Init(capacity);
			delete[] Arr;
		}
		else {
			Alloter.Init(capacity);
			Alloter.SetDynamicCapacityMode(true);
		}
		Arr = newArr;
	}

	int Alloc() {
		int index = Alloter.GetRecyleIndex();

		if (index >= 0) {
			goto ALLOC_CODE;
		}
		else {
			if (size >= Capacity) {
				Init(Capacity << 1);
			}
			index = size;
			Alloter.size += 1;
			goto ALLOC_CODE;
		}

	ALLOC_CODE:
		int pi = index >> 5;
		int ci = index & 31;
		ui32& flag = Alloter.AllocFlag[pi];
		flag |= (1 << ci);
		return index;
	}

	void Free(int index) {
		Alloter.Free(index);
	}

	void Release() {
		if (Arr) delete[] Arr;
		Alloter.Release();
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

	__forceinline int IncNum(int index) {
		return Alloter.IncNum(index);
	}
	__forceinline int InvIncNum(int index) {
		return Alloter.InvIncNum(index);
	}

	void GetTourPairs(indexRange* out, int* outlen) {
		Alloter.GetTourPairs(out, outlen);
	}
};

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