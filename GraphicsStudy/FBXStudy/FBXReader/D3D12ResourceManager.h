#ifndef D3D12_RESOURCEMANAGER_H
#define D3D12_RESOURCEMANAGER_H

#include <d3d12.h>

class CD3D12ResourceManager {
	ID3D12Device5* m_pD3DDevice = nullptr;
	ID3D12CommandQueue* m_pCommandQueue = nullptr;
	ID3D12CommandAllocator* m_pCommandAllocator = nullptr;
	ID3D12GraphicsCommandList* m_pCommandList = nullptr;
	
	HANDLE m_hFenceEvent = nullptr;
	ID3D12Fence* m_pFence = nullptr;
	UINT64 m_ui64FenceValue = 0;

	void CreateFence();
	void CleanupFence();
	void CreateCommandList();
	void CleanupCommandList();

	UINT64 Fence();
	void WaitForFenceValue();

	void Cleanup();

public:
	BOOL Initialize(ID3D12Device5* pD3DDevice);
	HRESULT CreateVertexBuffer(UINT SizePerVertex, DWORD dwVertexNum, D3D12_VERTEX_BUFFER_VIEW* pOutVertexBufferView, ID3D12Resource** ppOutBuffer, void* pInitData);
	HRESULT CreateIndexBuffer(DWORD dwIndexNum, D3D12_INDEX_BUFFER_VIEW* pOutIndexBufferView, ID3D12Resource** ppOutBuffer, void* pInitData);
	void UpdateTextureForWrite(ID3D12Resource* pDestTexResource, ID3D12Resource* pSrcTexResource);
	BOOL CreateTexture(ID3D12Resource** ppOutResource, UINT Width, UINT Height, DXGI_FORMAT format, const BYTE* pInitImage);

	CD3D12ResourceManager();
	~CD3D12ResourceManager();
};

#endif