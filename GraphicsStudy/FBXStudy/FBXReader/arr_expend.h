#ifndef H_ARR_EXPEND
#define H_ARR_EXPEND

#ifdef __cplusplus
//C++ Compiler"
#else
//This is not C++ compiler."
#endif

#ifdef __STDC__
//Standard C compatible compiler"
#endif

#if defined (_MSC_VER)
//This is Microsoft C/C++ compiler Ver.
#elif defined (__TURBOC__)
//This is Turbo C/C++ compiler Ver.
#elif defined (__BORLANDC__)
//This is Borland C/C++ compiler Ver.
#elif defined (__WATCOMC__)
//This is Watcom C/C++ compiler Ver.
#elif defined (__IBMCPP__)
//This is IBM Visual Age C++ compiler Ver.
#elif defined (__GNUC__)
//This is GNU C compiler Ver.
#else
//This is knowwn compiler.
#endif

#include <string.h>
#include <string>
#include <iostream>
using namespace std;

template<typename T> struct IsNumber
{
	static bool const value =
		(((std::is_same<T, si8>::value ||
			std::is_same<T, ui8>::value) ||
			(std::is_same<T, si16>::value ||
				std::is_same<T, ui16>::value)) ||
		((std::is_same<T, si32>::value ||
			std::is_same<T, ui32>::value) ||
			(std::is_same<T, si64>::value ||
				std::is_same<T, ui64>::value))) || 
		(std::is_same<T, float>::value || 
			std::is_same<T, double>::value);
};

class lcstr
{
public:
	char* Arr;
	size_t maxsize = 0;
	size_t up = 0;
	bool islocal = true;

	lcstr()
	{
		Arr = nullptr;
		maxsize = 0;
		up = 0;
		islocal = true;
	}

	virtual ~lcstr()
	{
		if (islocal) {
			delete[]Arr;
			Arr = nullptr;
		}
	}

	void NULLState()
	{
		Arr = nullptr;
		maxsize = 0;
		up = 0;
	}

	void Init(size_t siz, bool local)
	{
		islocal = local;
		char* newArr = new char[siz];
		if (Arr != nullptr)
		{
			for (int i = 0; i < maxsize; ++i)
			{
				newArr[i] = Arr[i];
			}

			delete[]Arr;
			Arr = nullptr;
		}

		Arr = newArr;
		maxsize = siz;
	}

	void operator=(const char* str)
	{
		int len = strlen(str) + 1;
		if (Arr == nullptr)
		{
			Arr = new char[len];
		}

		if (maxsize < len)
		{
			Init(len + 1, islocal);
		}
#if defined(__GNUC__)
		strcpy(Arr, str);
#elif defined (_MSC_VER)
		strcpy_s(Arr, len, str);
#endif
		up = len - 1;
	}

	bool operator==(char* str)
	{
		if (strcmp(Arr, str) == 0)
			return true;
		else
			return false;
	}

	bool operator==(const char* str)
	{
		if (strcmp(Arr, str) == 0)
			return true;
		else
			return false;
	}

	char& at(size_t i)
	{
		return Arr[i];
	}

	char* c_str()
	{
		Arr[up] = 0;
		return Arr;
	}

	char& operator[] (size_t i)
	{
		return Arr[i];
	}

	void push_back(char value)
	{
		if (up < maxsize)
		{
			Arr[up] = value;
			up += 1;
			Arr[up] = 0;
		}
		else
		{
			Init(maxsize * 2 + 1, islocal);
			Arr[up] = value;
			up += 1;
			Arr[up] = 0;
		}
	}

	void pop_back()
	{
		if (up - 1 >= 0)
		{
			up -= 1;
			Arr[up] = 0;
		}
	}

	void erase(size_t i)
	{
		for (int k = i; k < up; ++k)
		{
			Arr[k] = Arr[k + 1];
		}
		up -= 1;
	}

	void insert(size_t i, char value)
	{
		push_back(value);
		for (int k = maxsize - 1; k > i; k--)
		{
			Arr[k] = Arr[k - 1];
		}
		Arr[i] = value;
	}

	size_t size()
	{
		return up;
	}

	void clear()
	{
		if (Arr != nullptr)
			delete[]Arr;
		Arr = nullptr;
		up = 0;

		Init(2, islocal);
	}

	void release()
	{
		if (Arr != nullptr)
			delete[]Arr;
		Arr = nullptr;
		up = 0;
		islocal = false;
	}

	static lcstr& to_string(int n) {
		string str = std::to_string(n);
		lcstr r;
		r.NULLState();
		r.Init(8, false);
		for (int i = 0; i < str.size(); ++i) {
			r.push_back(str[i]);
		}
		return r;
	}

	void operator+=(char* str) {
		for (int i = 0; i < strlen(str); ++i) {
			push_back(str[i]);
		}
	}
};

class lwstr
{
public:
	wchar_t* Arr;
	size_t maxsize = 0;
	size_t up = 0;
	bool islocal = true;

	lwstr()
	{
		Arr = nullptr;
		maxsize = 0;
		up = 0;
		islocal = true;
	}

	virtual ~lwstr()
	{
		if (islocal) {
			delete[]Arr;
			Arr = nullptr;
		}
	}

	void NULLState()
	{
		Arr = nullptr;
		maxsize = 0;
		up = 0;
	}

	void Init(size_t siz, bool local)
	{
		islocal = local;
		wchar_t* newArr = new wchar_t[siz];
		if (Arr != nullptr)
		{
			for (int i = 0; i < maxsize; ++i)
			{
				newArr[i] = Arr[i];
			}

			delete[]Arr;
			Arr = nullptr;
		}

		Arr = newArr;
		maxsize = siz;
	}

	void operator=(const wchar_t* str)
	{
		int len = wcslen(str) + 1;
		if (Arr == nullptr)
		{
			Arr = new wchar_t[len];
		}

		if (maxsize < len)
		{
			Init(len + 1, islocal);
		}

#if defined(__GNUC__)
		wcscpy(Arr, str);
#elif defined (_MSC_VER)
		wcscpy_s(Arr, len, str);
#endif
		up = len - 1;
	}

	bool operator==(wchar_t* str)
	{
		if (wcscmp(Arr, str) == 0)
			return true;
		else
			return false;
	}

	bool operator==(const wchar_t* str)
	{
		if (wcscmp(Arr, str) == 0)
			return true;
		else
			return false;
	}

	wchar_t& at(size_t i)
	{
		return Arr[i];
	}

	wchar_t* c_str()
	{
		Arr[up] = 0;
		return Arr;
	}

	wchar_t& operator[] (size_t i)
	{
		return Arr[i];
	}

	void push_back(wchar_t value)
	{
		if (up < maxsize)
		{
			Arr[up] = value;
			up += 1;
			Arr[up] = 0;
		}
		else
		{
			Init(maxsize * 2 + 1, islocal);
			Arr[up] = value;
			up += 1;
			Arr[up] = 0;
		}
	}

	void pop_back()
	{
		if (up - 1 >= 0)
		{
			up -= 1;
			Arr[up] = 0;
		}
	}

	void erase(size_t i)
	{
		for (int k = i; k < up; ++k)
		{
			Arr[k] = Arr[k + 1];
		}
		up -= 1;
	}

	void insert(size_t i, wchar_t value)
	{
		push_back(value);
		for (int k = maxsize - 1; k > i; k--)
		{
			Arr[k] = Arr[k - 1];
		}
		Arr[i] = value;
	}

	size_t size()
	{
		return up;
	}

	void clear()
	{
		if (Arr != nullptr)
			delete[]Arr;
		Arr = nullptr;
		up = 0;

		Init(2, islocal);
	}

	void release()
	{
		if (Arr != nullptr)
			delete[]Arr;
		Arr = nullptr;
		up = 0;
		islocal = false;
	}

	static lwstr& to_string(int n) {
		wstring str = std::to_wstring(n);
		lwstr r;
		r.NULLState();
		r.Init((str.size() + 1), false);
		for (int i = 0; i < str.size(); ++i) {
			r.push_back(str[i]);
		}
		return r;
	}

	void operator+=(wchar_t* str) {
		for (int i = 0; i < wcslen(str); ++i) {
			push_back(str[i]);
		}
	}
};

//siz : 16 byte
template < typename T > class vecarr
{
public:
	T* Arr;
	size_t maxsize = 0;
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

	void Init(size_t siz)
	{
		T* newArr = new T[siz];
		if (Arr != nullptr)
		{
			for (int i = 0; i < maxsize; ++i)
			{
				newArr[i] = Arr[i];
			}

			delete[]Arr;
			Arr = nullptr;
		}

		Arr = newArr;
		maxsize = siz;
	}

	T& at(size_t i)
	{
		return Arr[i];
	}

	T& operator[](size_t i)
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

	void erase(size_t i)
	{
		for (int k = i; k < up; ++k)
		{
			Arr[k] = Arr[k + 1];
		}
		up -= 1;
	}

	void insert(size_t i, T value)
	{
		push_back(value);
		for (int k = maxsize - 1; k > i; k--)
		{
			Arr[k] = Arr[k - 1];
		}
		Arr[i] = value;
	}

	size_t size()
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
#endif