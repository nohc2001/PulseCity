//This is Noh Hun Chueul's Original Code. You Cannot Copy it.

#pragma once
#include <iostream>
using namespace std;

typedef unsigned char byte8;
typedef unsigned char ui8;
typedef unsigned short ui16;
typedef unsigned int ui32;
typedef unsigned long long ui64;
typedef char si8;
typedef short si16;
typedef int si32;
typedef long long si64;

//siz : 16 byte
template < typename T > class vecarr
{
public:
	T* Arr;
	ui32 maxsize = 0;
	int up = 0;
	//bool islocal = true;

	vecarr()
	{
		Arr = nullptr;
		maxsize = 0;
		up = 0;
		//islocal = true;
	}

	~vecarr()
	{
	}

	void NULLState()
	{
		Arr = nullptr;
		maxsize = 0;
		up = 0;
	}

	void Init(ui32 siz)
	{
		T* newArr = new T[siz];
		unsigned int msiz = min(siz, maxsize);
		if (Arr != nullptr)
		{
			for (int i = 0; i < msiz; ++i)
			{
				newArr[i] = Arr[i];
			}

			delete[]Arr;
			Arr = nullptr;
		}

		Arr = newArr;
		maxsize = siz;
	}

	T& at(ui32 i)
	{
		return Arr[i];
	}

	T& operator[](ui32 i)
	{
		return Arr[i];
	}

	void push_back(T value)
	{
		if (up < maxsize)
		{
			Arr[up] = value;
			up += 1;
		}
		else
		{
			Init(maxsize * 2);
			Arr[up] = value;
			up += 1;
		}
	}

	void pop_back()
	{
		if (up - 1 >= 0)
		{
			up -= 1;
			//Arr[up] = 0;
		}
	}

	void erase(ui32 i)
	{
		for (int k = i; k < up; ++k)
		{
			Arr[k] = Arr[k + 1];
		}
		up -= 1;
	}

	void insert(ui32 i, T value)
	{
		push_back(value);
		for (int k = maxsize - 1; k > i; k--)
		{
			Arr[k] = Arr[k - 1];
		}
		Arr[i] = value;
	}

	ui32 size()
	{
		return up;
	}

	void clear()
	{
		if (Arr != nullptr)
			delete[]Arr;
		Arr = nullptr;
		up = 0;

		Init(2);
	}

	T& last() {
		if (up > 0) {
			return Arr[up - 1];
		}
		return Arr[0];
	}

	void release()
	{
		if (Arr != nullptr)
			delete[]Arr;
		Arr = nullptr;
		up = 0;
	}
};

template<typename T> struct IsInteger
{
	static bool const value =
		((std::is_same<T, si8>::value ||
			std::is_same<T, ui8>::value) ||
			(std::is_same<T, si16>::value ||
				std::is_same<T, ui16>::value)) ||
		((std::is_same<T, si32>::value ||
			std::is_same<T, ui32>::value) ||
			(std::is_same<T, si64>::value ||
				std::is_same<T, ui64>::value));
};

template<typename T> struct IsRealNumber
{
	static bool const value =
		std::is_same<T, float>::value || std::is_same<T, double>::value;
};

typedef struct VP
{
	char mod = 0;		// mod 0:value 1:ptr
	int* ptr = nullptr; // arrgraph ptr or T ptr
};

template <typename IndexType, typename ValueType>
struct range
{
	IndexType end; // 범위의 인덱스 최대값
	ValueType value; // 해당 범위에 값
};

struct RangeArrTourContext {
	vecarr<int> indexarr;
	void* ValuePtr;
};

template <typename IndexType, typename ValueType>
struct RangeArr {
	unsigned int divn; // 범위 데이터들을 divn 등분함.
	IndexType minI; // 정의역 최소값
	IndexType maxI; // 정의역 최대값
	IndexType cmax; // 배열의 요소 하나가 포함하는 인덱스범위의 길이
	void* child; // 배열의 주소. ValueType[] or RangeArr<IndexType, ValueType>[]

	ValueType& at(IndexType index);
	ValueType& operator[](IndexType index);

	void print_state(ofstream& ofs);
	void clean();
	void release();

	RangeArrTourContext GetTourStartPoint();
	RangeArrTourContext TourNext(RangeArrTourContext cxt);
};

template <typename IndexType, typename ValueType>
struct RangeArr_node {
	unsigned short divn; // 범위 데이터들을 divn 등분함.
	// 1 -> only value, 2 -> two value arr, else rangearr
	IndexType maxI; // 정의역 최대값
	IndexType cmax; // cmax = (maxI + 1) / divn;
	void* child; // 데이터의 주소

	void print_state(ofstream& ofs, int stack = 0) {
		switch (divn) {
		case 1:
			ofs << "divn : " << divn << " | maxI : " << maxI << " | cmax : " << cmax << " | Value : " << *((ValueType*)child) << endl;
			return;
		case 2:
			ofs << "divn : " << divn << " | maxI : " << maxI << " | cmax : " << cmax << " | Value[0] : " << ((ValueType*)child)[0] << " | Value[1] : " << ((ValueType*)child)[1] << endl;
			return;
		}
		ofs << "divn : " << divn << " | maxI : " << maxI << " | cmax : " << cmax << endl;
		for (int i = 0; i < divn; ++i) {
			RangeArr_node ran = ((RangeArr_node*)child)[i];
			for (int i = 0; i < stack + 1; ++i) {
				ofs << "\t";
			}
			ofs << "\tchild[" << i << "] : ";
			ran.print_state(ofs, stack + 1);
		}
	}

	// -1 : no replace
	// 0~1 : replace with that index
	int clean(vecarr<RangeArr_node<IndexType, ValueType>*>& rangeBuff) {
		switch (divn) {
		case 1:
		case 2:
		{
			for (int i = 0; i < rangeBuff.size(); ++i) {
				RangeArr_node<IndexType, ValueType>* ri = rangeBuff.at(i);
				if (*ri == *this) {
					return i;
				}
			}

			// no replace
			rangeBuff.push_back(this);
			return -1;
		}
		}

		for (int i = 0; i < divn; ++i) {
			RangeArr_node<IndexType, ValueType>& ran = ((RangeArr_node<IndexType, ValueType>*)child)[i];
			int index = ran.clean(rangeBuff);
			if (index >= 0) {
				if (ran.divn == 2) {
					free(ran.child); // 2siz valuetype
				}
				else if (ran.divn == 1) {
					free(ran.child); // 1siz valuetype
				}
				else if (ran.divn > 2) {
					free(ran.child); // divn siz value type
				}
				ran = *rangeBuff.at(index);
			}
		}

		for (int i = 0; i < rangeBuff.size(); ++i) {
			RangeArr_node<IndexType, ValueType>* ri = rangeBuff.at(i);
			if (ri->divn == divn) {
				bool b = ri->maxI == maxI;
				b = b && ri->cmax == cmax;
				for (int k = 0; k < divn; ++k) {
					b = b && (((RangeArr_node<IndexType, ValueType>*)ri->child)[k] == ((RangeArr_node<IndexType, ValueType>*)child)[k]);
				}

				if (b) {
					return i;
				}
				else {
					rangeBuff.push_back(this);
					return -1;
				}
			}
		}

		return -1;
	}

	bool operator==(RangeArr_node<IndexType, ValueType>& ref) {
		bool b = (divn == ref.divn);
		b = b && (maxI == ref.maxI);
		b = b && (cmax == ref.cmax);
		b = b && (child == ref.child);
		return b;
	}

	void release() {
		if (divn > 2) {
			RangeArr_node<IndexType, ValueType>* ptr = (RangeArr_node<IndexType, ValueType>*)child;
			for (int i = 0; i < divn; ++i) {
				ptr->release();
				ptr += 1;
			}

			free(child); // size : sizeof(RangeArr_node<IndexType, ValueType>) * divn
		}
		else {
			if (divn == 2) {
				free(child); // sizeof(ValueType) * 2
			}
			else if (divn == 1) {
				free(child); // sizeof(ValueType)
			}
		}
	}
};

template <typename IndexType, typename ValueType>
__forceinline ValueType& at_forceinline(RangeArr<IndexType, ValueType>* self, enable_if_t<IsInteger<IndexType>::value, IndexType> index) {
	ValueType v;
	if (self->minI > index || index > self->maxI) return v;
	IndexType previndex = index - self->minI;
	int movindex = (int)(previndex / self->cmax);
	RangeArr_node<IndexType, ValueType> ran = ((RangeArr_node<IndexType, ValueType>*)self->child)[movindex];
	previndex -= (IndexType)movindex * self->cmax;
RANGEARR_ACCESSOPERATION_LOOP:
	switch (ran.divn) {
	case 1:
		return *(ValueType*)ran.child;
	case 2:
		previndex += ran.maxI;
		movindex = previndex / ran.cmax;
		return ((ValueType*)ran.child)[movindex];
		break;
	}
	movindex = previndex / ran.cmax;
	previndex -= movindex * ran.cmax;
	ran = ((RangeArr_node<IndexType, ValueType>*)ran.child)[movindex];
	goto RANGEARR_ACCESSOPERATION_LOOP;
}

template <typename IndexType, typename ValueType>
__forceinline ValueType& at_forceinline(RangeArr<IndexType, ValueType>* self, enable_if_t<IsRealNumber<IndexType>::value, IndexType> index) {
	if (self->minI > index || index >= self->maxI) {
		ValueType v;
		ui8* vptr = (ui8*)&v;
		for (int i = 0; i < sizeof(ValueType); ++i) {
			*vptr = 0;
			vptr += 1;
		}
		return v;
	}
	IndexType previndex = index - self->minI;
	int movindex = (int)(previndex / self->cmax);
	RangeArr_node<IndexType, ValueType> ran = ((RangeArr_node<IndexType, ValueType>*)self->child)[movindex];
	previndex -= (IndexType)movindex * self->cmax;
RANGEARR_ACCESSOPERATION_LOOP:
	switch (ran.divn) {
	case 1:
		return *(ValueType*)ran.child;
	case 2:
		previndex += ran.maxI;
		movindex = previndex / ran.cmax;
		return ((ValueType*)ran.child)[movindex];
		break;
	}
	movindex = previndex / ran.cmax;
	previndex -= movindex * ran.cmax;
	ran = ((RangeArr_node<IndexType, ValueType>*)ran.child)[movindex];
	goto RANGEARR_ACCESSOPERATION_LOOP;
}

template <typename IndexType, typename ValueType>
ValueType& RangeArr<IndexType, ValueType>::at(IndexType index) {
	return at_forceinline<IndexType, ValueType>(this, index);
}

template <typename IndexType, typename ValueType>
ValueType& RangeArr<IndexType, ValueType>::operator[](IndexType index) {
	return at_forceinline<IndexType, ValueType>(this, index);
}

template <typename IndexType, typename ValueType>
void RangeArr<IndexType, ValueType>::release() {
	RangeArr_node<IndexType, ValueType>* ptr = (RangeArr_node<IndexType, ValueType>*)child;
	if (divn > 2) {
		for (int i = 0; i < divn; ++i) {
			ptr->release();
			ptr += 1;
		}
	}
}

template <typename IndexType, typename ValueType>
RangeArr_node<IndexType, ValueType> AddRangeArrNode(std::enable_if_t<IsInteger<IndexType>::value, vecarr<range<IndexType, ValueType>>> r) {
	RangeArr_node<IndexType, ValueType> darr;
	darr.divn = r.size();
	darr.maxI = r.last().end;

	// cmax set
	IndexType totalDomainLen = (darr.maxI + 1);
	IndexType temp = totalDomainLen % darr.divn;
	IndexType cmax = totalDomainLen / darr.divn;
	IndexType mov_mini = 0;
	if (temp != 0) {
		cmax += 1;
	}

	//divn set
	temp = totalDomainLen % cmax;
	IndexType dn = totalDomainLen / cmax;
	if (temp != 0) {
		dn += 1;
	}
	darr.divn = dn;
	darr.cmax = cmax;
	darr.child = nullptr;

	unsigned int range_counter = 0;
	RangeArr_node<IndexType, ValueType>* CArr = (RangeArr_node<IndexType, ValueType>*)malloc(sizeof(RangeArr_node<IndexType, ValueType>) * darr.divn);
	if (CArr == nullptr) return darr;
	for (int i = 0; i < darr.divn; ++i) {
		CArr[i].maxI = cmax - 1;
		unsigned int cr_count = 0;
		unsigned int r_start = range_counter;
		for (int k = range_counter; k < r.size(); ++k) {
			range<IndexType, ValueType> ran = r.at(k);
			if (ran.end <= cmax * (i + 1) - 1) {
				range_counter += 1;
				cr_count += 1;
				if (ran.end == cmax * (i + 1) - 1) {
					goto ADD_DYNAMICARR_ADD_CDARR;
				}
			}
			else {
				cr_count += 1;
				goto ADD_DYNAMICARR_ADD_CDARR;
			}
		}

	ADD_DYNAMICARR_ADD_CDARR:
		if (cr_count > 2) {
			vecarr<range<IndexType, ValueType>> r2;
			r2.NULLState();
			r2.Init(cr_count);
			/*CArr[i].divn = cr_count;*/
			for (int u = 0; u < cr_count; ++u) {
				range<IndexType, ValueType> tempR = r.at(r_start + u);
				tempR.end -= cmax * i;
				//r2.push_back(r.at(r_start + u));
				r2.push_back(tempR);
			}
			/*RangeArr_node<IndexType, ValueType>* ccarr = (RangeArr_node<IndexType, ValueType>*)malloc(sizeof(RangeArr_node<IndexType, ValueType>));
			*ccarr = AddRangeArrNode(r2);
			CArr[i].child = (void*)ccarr;*/
			CArr[i] = AddRangeArrNode<IndexType, ValueType>(r2);
			r2.release();
		}
		else if (cr_count == 2) {
			vecarr<range<IndexType, ValueType>> r2;
			r2.NULLState();
			r2.Init(cr_count);
			CArr[i].divn = cr_count;
			for (int u = 0; u < cr_count; ++u) {
				range<IndexType, ValueType> tempR = r.at(r_start + u);
				if (tempR.end > (i + 1) * darr.cmax) {
					tempR.end = (i + 1) * darr.cmax;
				}
				tempR.end -= cmax * i;
				r2.push_back(tempR);
			}

			//RangeArr_node<IndexType, ValueType>* ccarr = (RangeArr_node<IndexType, ValueType>*)malloc(sizeof(RangeArr_node<IndexType, ValueType>));
			CArr[i].divn = 2;

			IndexType start = 0;
			IndexType end = darr.cmax - 1;
			IndexType margin_right = end - r2[0].end;
			IndexType margin_left = r2[0].end - start + 1;
			if (margin_left > margin_right) {
				CArr[i].cmax = margin_left;
				CArr[i].maxI = 0;
			}
			else {
				CArr[i].cmax = margin_right;
				CArr[i].maxI = r2[0].end - margin_right + 1;
				CArr[i].maxI = start - CArr[i].maxI;
			}

			ValueType* varr = (ValueType*)malloc(sizeof(ValueType) * 2);
			if (varr != nullptr) {
				varr[0] = r2[0].value;
				varr[1] = r2[1].value;
			}
			CArr[i].child = (void*)varr;
			r2.release();
		}
		else {
			CArr[i].divn = 1;
			CArr[i].cmax = 1;
			CArr[i].maxI = 0;
			ValueType* varr = (ValueType*)malloc(sizeof(ValueType));
			if (varr != nullptr) {
				*varr = r.at(r_start).value;
			}
			CArr[i].child = varr;
		}
	}

	darr.child = CArr;
	return darr;
}

template <typename IndexType, typename ValueType>
RangeArr<IndexType, ValueType> AddRangeArr(IndexType mini, std::enable_if_t<IsInteger<IndexType>::value, vecarr<range<IndexType, ValueType>>> r) {
	RangeArr<IndexType, ValueType> darr;
	darr.divn = r.size();
	darr.minI = mini;
	darr.maxI = r.last().end;

	//cmax set
	IndexType totalDomainLen = (darr.maxI - darr.minI + 1);
	IndexType temp = totalDomainLen % darr.divn;
	IndexType cmax = totalDomainLen / darr.divn;
	IndexType mov_mini = mini;
	if (temp != 0) {
		cmax += 1;
	}

	//divn set
	temp = totalDomainLen % cmax;
	IndexType dn = totalDomainLen / cmax;
	if (temp != 0) {
		dn += 1;
	}
	darr.divn = dn;
	darr.cmax = cmax;
	darr.child = nullptr;

	unsigned int range_counter = 0;
	RangeArr_node<IndexType, ValueType>* CArr = (RangeArr_node<IndexType, ValueType>*)malloc(sizeof(RangeArr_node<IndexType, ValueType>) * darr.divn);

	if (CArr == nullptr) return darr;
	for (int i = 0; i < darr.divn; ++i) {
		CArr[i].maxI = cmax - 1;
		unsigned int cr_count = 0;
		unsigned int r_start = range_counter;
		for (int k = range_counter; k < r.size(); ++k) {
			range<IndexType, ValueType> ran = r.at(k);
			if (ran.end <= darr.minI + cmax * (i + 1) - 1) {
				range_counter += 1;
				cr_count += 1;
				if (ran.end == darr.minI + cmax * (i + 1) - 1) {
					goto ADD_DYNAMICARR_ADD_CDARR;
				}
			}
			else {
				cr_count += 1;
				goto ADD_DYNAMICARR_ADD_CDARR;
			}
		}

	ADD_DYNAMICARR_ADD_CDARR:
		if (cr_count > 2) {
			vecarr<range<IndexType, ValueType>> r2;
			r2.NULLState();
			r2.Init(cr_count);
			//CArr[i].divn = cr_count;
			for (int u = 0; u < cr_count; ++u) {
				range<IndexType, ValueType> tempR = r.at(r_start + u);
				tempR.end -= darr.minI + cmax * i;
				r2.push_back(tempR);
			}
			//RangeArr_node<IndexType, ValueType>* ccarr = (RangeArr_node<IndexType, ValueType>*)malloc(sizeof(RangeArr_node<IndexType, ValueType>));
			//*ccarr = 
			//CArr[i].child = (void*)ccarr;
			CArr[i] = AddRangeArrNode<IndexType, ValueType>(r2);
			r2.release();
		}
		else if (cr_count == 2) {
			vecarr<range<IndexType, ValueType>> r2;
			r2.NULLState();
			r2.Init(cr_count);
			CArr[i].divn = cr_count;
			for (int u = 0; u < cr_count; ++u) {
				range<IndexType, ValueType> tempR = r.at(r_start + u);
				if (tempR.end > darr.minI + (i + 1) * darr.cmax) {
					tempR.end = (i + 1) * darr.cmax;
				}
				tempR.end -= darr.minI + cmax * i;
				r2.push_back(tempR);
			}

			//RangeArr_node<IndexType, ValueType>* ccarr = (RangeArr_node<IndexType, ValueType>*)malloc(sizeof(RangeArr_node<IndexType, ValueType>));
			CArr[i].divn = 2;

			IndexType start = 0;
			IndexType end = darr.cmax - 1;
			IndexType margin_right = end - r2[0].end;
			IndexType margin_left = r2[0].end - start + 1;
			if (margin_left > margin_right) {
				CArr[i].cmax = margin_left;
				CArr[i].maxI = 0;
			}
			else {
				CArr[i].cmax = margin_right;
				CArr[i].maxI = r2[0].end - margin_right + 1;
				CArr[i].maxI = start - CArr[i].maxI;
			}

			ValueType* varr = (ValueType*)malloc(sizeof(ValueType) * 2);
			if (varr != nullptr) {
				varr[0] = r2[0].value;
				varr[1] = r2[1].value;
				CArr[i].child = (void*)varr;
			}
			r2.release();
		}
		else {
			CArr[i].divn = 1;
			CArr[i].cmax = 1;
			CArr[i].maxI = 0;
			ValueType* varr = (ValueType*)malloc(sizeof(ValueType));
			if (varr != nullptr) {
				*varr = r.at(r_start).value;
			}
			CArr[i].child = varr;
		}
	}

	darr.child = CArr;
	return darr;
}

template <typename IndexType, typename ValueType>
RangeArr_node<IndexType, ValueType> AddRangeArrNode(std::enable_if_t<IsRealNumber<IndexType>::value, vecarr<range<IndexType, ValueType>>> r) {
	RangeArr_node<IndexType, ValueType> darr;
	darr.divn = r.size();
	darr.maxI = r.last().end;

	/*
	// cmax set
	IndexType totalDomainLen = (darr.maxI + 1);
	IndexType sub = 0;
	if (std::is_same<IndexType, float>::value) {
		sub = darr.divn * floorf(totalDomainLen / darr.divn);
	}
	else if (std::is_same<IndexType, double>::value) {
		sub = darr.divn * floor(totalDomainLen / darr.divn);
	}
	IndexType temp = totalDomainLen - sub; // totalDomainLen % darr.divn;
	IndexType cmax = totalDomainLen / darr.divn;
	IndexType mov_mini = 0;
	if (temp >= 1) {
		cmax += 1;
	}

	//divn set
	if (std::is_same<IndexType, float>::value) {
		sub = cmax * floorf(totalDomainLen / cmax);
	}
	else if (std::is_same<IndexType, double>::value) {
		sub = cmax * floor(totalDomainLen / cmax);
	}
	temp = totalDomainLen - sub;
	IndexType dn = totalDomainLen / cmax;
	if (temp >= 1) {
		dn += 1;
	}
	darr.divn = dn;
	darr.cmax = cmax;
	darr.child = nullptr;
	*/
	darr.cmax = (darr.maxI) / darr.divn;
	darr.child = nullptr;

	unsigned int range_counter = 0;
	RangeArr_node<IndexType, ValueType>* CArr = (RangeArr_node<IndexType, ValueType>*)malloc(sizeof(RangeArr_node<IndexType, ValueType>) * darr.divn);
	if (CArr == nullptr) return darr;
	for (int i = 0; i < darr.divn; ++i) {
		CArr[i].maxI = darr.cmax;
		unsigned int cr_count = 0;
		unsigned int r_start = range_counter;
		for (int k = range_counter; k < r.size(); ++k) {
			range<IndexType, ValueType> ran = r.at(k);
			if (ran.end <= darr.cmax * (i + 1)) {
				range_counter += 1;
				cr_count += 1;
				if (ran.end >= darr.cmax * (i + 1)) {
					goto ADD_DYNAMICARR_ADD_CDARR;
				}
			}
			else {
				cr_count += 1;
				goto ADD_DYNAMICARR_ADD_CDARR;
			}
		}

	ADD_DYNAMICARR_ADD_CDARR:
		if (cr_count > 2) {
			vecarr<range<IndexType, ValueType>> r2;
			r2.NULLState();
			r2.Init(cr_count);
			/*CArr[i].divn = cr_count;*/
			for (int u = 0; u < cr_count; ++u) {
				range<IndexType, ValueType> tempR = r.at(r_start + u);
				if (tempR.end > (i + 1) * darr.cmax) {
					tempR.end = (i + 1) * darr.cmax;
				}
				tempR.end -= darr.cmax * i;
				//r2.push_back(r.at(r_start + u));
				r2.push_back(tempR);
			}
			/*RangeArr_node<IndexType, ValueType>* ccarr = (RangeArr_node<IndexType, ValueType>*)malloc(sizeof(RangeArr_node<IndexType, ValueType>));
			*ccarr = AddRangeArrNode(r2);
			CArr[i].child = (void*)ccarr;*/
			CArr[i] = AddRangeArrNode<IndexType, ValueType>(r2);
			r2.release();
		}
		else if (cr_count == 2) {
			vecarr<range<IndexType, ValueType>> r2;
			r2.NULLState();
			r2.Init(cr_count);
			CArr[i].divn = cr_count;
			for (int u = 0; u < cr_count; ++u) {
				range<IndexType, ValueType> tempR = r.at(r_start + u);
				tempR.end -= darr.cmax * i;
				r2.push_back(tempR);
			}

			//RangeArr_node<IndexType, ValueType>* ccarr = (RangeArr_node<IndexType, ValueType>*)malloc(sizeof(RangeArr_node<IndexType, ValueType>));
			CArr[i].divn = 2;

			IndexType start = 0;
			IndexType end = darr.cmax;
			IndexType margin_right = end - r2[0].end;
			IndexType margin_left = r2[0].end - start;
			if (margin_left > margin_right) {
				CArr[i].cmax = margin_left;
				CArr[i].maxI = 0;
			}
			else {
				CArr[i].cmax = margin_right;
				CArr[i].maxI = r2[0].end - margin_right;
				CArr[i].maxI = start - CArr[i].maxI;
			}

			ValueType* varr = (ValueType*)malloc(sizeof(ValueType) * 2);
			if (varr != nullptr) {
				varr[0] = r2[0].value;
				varr[1] = r2[1].value;
			}
			CArr[i].child = (void*)varr;
			r2.release();
		}
		else {
			CArr[i].divn = 1;
			CArr[i].cmax = 1;
			CArr[i].maxI = 0;
			ValueType* varr = (ValueType*)malloc(sizeof(ValueType));
			if (varr != nullptr) {
				*varr = r.at(r_start).value;
			}
			CArr[i].child = varr;
		}
	}

	darr.child = CArr;
	return darr;
}

template <typename IndexType, typename ValueType>
RangeArr<IndexType, ValueType> AddRangeArr(IndexType mini, std::enable_if_t<IsRealNumber<IndexType>::value, vecarr<range<IndexType, ValueType>>> r) {
	RangeArr<IndexType, ValueType> darr;
	darr.divn = r.size();
	darr.minI = mini;
	darr.maxI = r.last().end;

	//cmax set
	IndexType totalDomainLen = (darr.maxI - darr.minI);
	IndexType cmax = totalDomainLen / darr.divn;
	IndexType mov_mini = mini;

	//divn set
	darr.cmax = cmax;
	darr.child = nullptr;

	unsigned int range_counter = 0;
	RangeArr_node<IndexType, ValueType>* CArr = (RangeArr_node<IndexType, ValueType>*)malloc(sizeof(RangeArr_node<IndexType, ValueType>) * darr.divn);

	if (CArr == nullptr) return darr;
	for (int i = 0; i < darr.divn; ++i) {
		CArr[i].maxI = cmax;
		unsigned int cr_count = 0;
		unsigned int r_start = range_counter;
		for (int k = range_counter; k < r.size(); ++k) {
			range<IndexType, ValueType> ran = r.at(k);
			if (ran.end <= darr.minI + cmax * (i + 1)) {
				range_counter += 1;
				cr_count += 1;
				if (ran.end == darr.minI + cmax * (i + 1)) {
					goto ADD_DYNAMICARR_ADD_CDARR;
				}
			}
			else {
				cr_count += 1;
				goto ADD_DYNAMICARR_ADD_CDARR;
			}
		}

	ADD_DYNAMICARR_ADD_CDARR:
		if (cr_count > 2) {
			vecarr<range<IndexType, ValueType>> r2;
			r2.NULLState();
			r2.Init(cr_count);
			//CArr[i].divn = cr_count;
			for (int u = 0; u < cr_count; ++u) {
				range<IndexType, ValueType> tempR = r.at(r_start + u);
				if (tempR.end > darr.minI + (i + 1) * darr.cmax) {
					tempR.end = darr.minI + (i + 1) * darr.cmax;
				}
				tempR.end -= darr.minI + cmax * i;
				r2.push_back(tempR);
			}
			//RangeArr_node<IndexType, ValueType>* ccarr = (RangeArr_node<IndexType, ValueType>*)malloc(sizeof(RangeArr_node<IndexType, ValueType>));
			//*ccarr = 
			//CArr[i].child = (void*)ccarr;
			CArr[i] = AddRangeArrNode<IndexType, ValueType>(r2);
			r2.release();
		}
		else if (cr_count == 2) {
			vecarr<range<IndexType, ValueType>> r2;
			r2.NULLState();
			r2.Init(cr_count);
			CArr[i].divn = cr_count;
			for (int u = 0; u < cr_count; ++u) {
				range<IndexType, ValueType> tempR = r.at(r_start + u);
				tempR.end -= darr.minI + cmax * i;
				r2.push_back(tempR);
			}

			//RangeArr_node<IndexType, ValueType>* ccarr = (RangeArr_node<IndexType, ValueType>*)malloc(sizeof(RangeArr_node<IndexType, ValueType>));
			CArr[i].divn = 2;

			IndexType start = 0;
			IndexType end = darr.cmax;
			IndexType margin_right = end - r2[0].end;
			IndexType margin_left = r2[0].end - start;
			if (margin_left > margin_right) {
				CArr[i].cmax = margin_left;
				CArr[i].maxI = 0;
			}
			else {
				CArr[i].cmax = margin_right;
				CArr[i].maxI = r2[0].end - margin_right;
				CArr[i].maxI = start - CArr[i].maxI;
			}

			ValueType* varr = (ValueType*)malloc(sizeof(ValueType) * 2);
			if (varr != nullptr) {
				varr[0] = r2[0].value;
				varr[1] = r2[1].value;
				CArr[i].child = (void*)varr;
			}
			r2.release();
		}
		else {
			CArr[i].divn = 1;
			CArr[i].cmax = 1;
			CArr[i].maxI = 0;
			ValueType* varr = (ValueType*)malloc(sizeof(ValueType));
			if (varr != nullptr) {
				*varr = r.at(r_start).value;
			}
			CArr[i].child = varr;
		}
	}

	darr.child = CArr;
	return darr;
}

template <typename IndexType, typename ValueType>
void RangeArr<IndexType, ValueType>::print_state(ofstream& ofs) {
	ofs << "divn : " << divn << " | minI : " << minI << " | maxI : " << maxI << " | cmax : " << cmax << endl;
	for (int i = 0; i < divn; ++i) {
		RangeArr_node<IndexType, ValueType> rad = ((RangeArr_node<IndexType, ValueType>*)child)[i];
		ofs << "\tchild[" << i << "] : ";
		rad.print_state(ofs);
	}
}

template <typename IndexType, typename ValueType>
void RangeArr<IndexType, ValueType>::clean() {
	vecarr<RangeArr_node<IndexType, ValueType>*> rangeBuff;
	rangeBuff.NULLState();
	rangeBuff.Init(8);
	for (int i = 0; i < divn; ++i) {
		RangeArr_node<IndexType, ValueType>& ran = ((RangeArr_node<IndexType, ValueType>*)child)[i];
		int index = ran.clean(rangeBuff);
		if (index >= 0) {
			if (ran.divn == 2) {
				free(ran.child); // 2siz valuetype
			}
			else if (ran.divn == 1) {
				free(ran.child); // 1siz valuetype
			}
			ran.child = rangeBuff.at(index)->child;
		}
	}
	rangeBuff.release();
}

template <typename IndexType, typename ValueType>
RangeArrTourContext RangeArr<IndexType, ValueType>::GetTourStartPoint() {
	RangeArrTourContext r;
	r.indexarr.NULLState();
	r.indexarr.Init(8);
	r.ValuePtr = nullptr;
	RangeArr_node<IndexType, ValueType>* ptr = child;
	if (ptr->divn > 2) {
		r.indexarr.push_back(0);
		ptr = ptr->child;
	}
	else {
		r.indexarr.push_back(0);
		r.ValuePtr = ptr->child;
	}

	return r;
}

template <typename IndexType, typename ValueType>
RangeArrTourContext RangeArr<IndexType, ValueType>::TourNext(RangeArrTourContext cxt) {
	int i = 0;
	vecarr<RangeArr_node<IndexType, ValueType>*> ptr_stack;
	ptr_stack.NULLState();
	ptr_stack.Init(8);

	ptr_stack.push_back(((RangeArr_node<IndexType, ValueType>*)child)[cxt.indexarr.at(0)]);
	for (int i = 1; i < cxt.indexarr.size(); ++i) {
		ptr_stack.push_back(((RangeArr_node<IndexType, ValueType>*)ptr_stack.last()->child)[cxt.indexarr.at(i)]);
	}

	RangeArr_node<IndexType, ValueType>* lastSeg = ptr_stack.at(ptr_stack.size() - 2);
	int aindex = cxt.indexarr.size() - 1;

RANGEARR_TOURNEXT_LOOP:
	if (cxt.indexarr.at(aindex) + 1 < lastSeg.divn) {
		cxt.indexarr.at(aindex) += 1;
		if (aindex == cxt.indexarr.size() - 1) {
			cxt.ValuePtr = ((ValueType*)lastSeg->child)[cxt.indexarr.at(aindex)];
		}
		else {
			for (int k = aindex + 1; k < cxt.indexarr.size(); ++k) {
				cxt.indexarr.at(k) = 0;
			}
		}
		ptr_stack.release();
		return cxt;
	}
	else {
		if (aindex == 0) {
			ptr_stack.release();
			RangeArrTourContext c;
			c.indexarr.NULLState();
			c.ValuePtr = nullptr;
			return c;
		}
		aindex -= 1;
		lastSeg = ptr_stack.at(aindex - 1);
		goto RANGEARR_TOURNEXT_LOOP;
	}
}

template <typename IndexType, typename ValueType>
ValueType RangeArrAccess(void* ptr, IndexType index) {
	return at_forceinline<IndexType, ValueType>((RangeArr<IndexType, ValueType>*)ptr, index);
}
