#pragma once

//#include "SIMD_Interface.h"

struct Descriptor256Allocator {
	static constexpr unsigned int ALLOC_MAX_COUNT = 256;
	static constexpr unsigned int RECYCLE_MAX_COUNT = 200;
	//static constexpr unsigned int BITFLAGBYTE_MAX_COUNT = 32;

	ID3D12Device* m_pD3DDevice = nullptr;
	ID3D12DescriptorHeap* m_pHeap = nullptr;
	UINT m_DescriptorSize = 32;
	short AllocPivot = 0;
	short RecyclePivot = 0;

	unsigned char recyleArr[RECYCLE_MAX_COUNT] = {};
	// 8 - sse 16 avx 32 avx256
	unsigned int BitFlags[8];
	// total 256byte

	BOOL	Initialize(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_FLAGS Flags);
	BOOL	AllocDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUHandle);
	void	FreeDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHandle);
	void	ChangeFlag(int location, bool is1);
	bool	GetFlag(int location);

	void	Release();
};