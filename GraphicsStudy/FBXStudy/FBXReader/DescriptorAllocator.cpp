#include "pch.h"
#include <d3d12.h>
#include "DescriptorAllocator.h"

BOOL Descriptor256Allocator::Initialize(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_FLAGS Flags)
{
	ZeroMemory(this, sizeof(Descriptor256Allocator));
	m_pD3DDevice = pDevice;
	m_pD3DDevice->AddRef();

	//D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	D3D12_DESCRIPTOR_HEAP_DESC commonHeapDesc = {};
	commonHeapDesc.NumDescriptors = ALLOC_MAX_COUNT;
	commonHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	commonHeapDesc.Flags = Flags;

	if (FAILED(m_pD3DDevice->CreateDescriptorHeap(&commonHeapDesc, IID_PPV_ARGS(&m_pHeap))))
	{
		__debugbreak();
	}

	m_DescriptorSize = m_pD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//CD3DX12_CPU_DESCRIPTOR_HANDLE commonHeapHandle(m_pHeap->GetCPUDescriptorHandleForHeapStart());

	//for (DWORD i=0; i<dwMaxCount; i++)
	//{
	//	commonHeapHandle.Offset(1, m_DescriptorSize);	
	//}

	AllocPivot = 0;
	RecyclePivot = 0;
	return TRUE;
}

BOOL Descriptor256Allocator::AllocDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUHandle)
{
	DWORD	dwIndex = 0;
	if (RecyclePivot > 0) {
		dwIndex = recyleArr[RecyclePivot - 1];
		while (RecyclePivot > 0 && dwIndex >= AllocPivot) {
			RecyclePivot -= 1;
			dwIndex = recyleArr[RecyclePivot - 1];
		}
		RecyclePivot -= 1;

		if (RecyclePivot == 0 && dwIndex >= AllocPivot) {
			goto ALLOC_PIVOT_UP;
		}
	}
	else {
		ALLOC_PIVOT_UP:
		if (AllocPivot + 1 < ALLOC_MAX_COUNT) {
			dwIndex = AllocPivot;
			AllocPivot += 1;
		}
		else {
			return FALSE;
		}
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHandle(m_pHeap->GetCPUDescriptorHandleForHeapStart(), dwIndex, m_DescriptorSize);
	*pOutCPUHandle = DescriptorHandle;
	ChangeFlag(dwIndex, true);
	return TRUE;
}

void Descriptor256Allocator::FreeDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHandle)
{
	D3D12_CPU_DESCRIPTOR_HANDLE base = m_pHeap->GetCPUDescriptorHandleForHeapStart();
#ifdef _DEBUG
	if (base.ptr > DescriptorHandle.ptr)
		__debugbreak();
#endif
	DWORD dwIndex = (DWORD)(DescriptorHandle.ptr - base.ptr) / m_DescriptorSize;
	ChangeFlag(dwIndex, false);
	if (dwIndex + 1 == AllocPivot) {
		AllocPivot -= 1;
		unsigned int pindex = dwIndex - 1;
		while (pindex >= 0 && GetFlag(pindex) == false) {
			AllocPivot -= 1;
			pindex -= 1;
		}
	}
	else {
		if (RecyclePivot + 1 < RECYCLE_MAX_COUNT) {
			recyleArr[RecyclePivot] = dwIndex;
			RecyclePivot += 1;
		}
	}
}

void Descriptor256Allocator::ChangeFlag(int location, bool is1)
{
	int bi = location >> 5;
	unsigned int si = 1 << (location & 31);
	if (is1) {
		BitFlags[bi] |= si;
	}
	else {
		si = ~si;
		BitFlags[bi] &= si;
	}
}

bool Descriptor256Allocator::GetFlag(int location)
{
	int bi = location >> 5;
	unsigned int si = 1 << (location & 31);
	return (BitFlags[bi] & si) != 0;
}

void Descriptor256Allocator::Release()
{
	if (m_pHeap)
	{
		m_pHeap->Release();
		m_pHeap = nullptr;
	}
	if (m_pD3DDevice)
	{
		m_pD3DDevice->Release();
		m_pD3DDevice = nullptr;
	}
}
