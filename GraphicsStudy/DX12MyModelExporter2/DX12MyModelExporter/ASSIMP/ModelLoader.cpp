#include "ModelLoader.h"
#include <fstream>

ModelLoader::ModelLoader() :
        dev_(nullptr),
        directory_(),
        textures_loaded_(),
        hwnd_(nullptr) {
    // empty
}


ModelLoader::~ModelLoader() {
    // empty
}

bool ModelLoader::Load(HWND hwnd, ID3D11Device * dev, ID3D11DeviceContext * devcon, std::string filename) {
	Assimp::Importer importer;

	const aiScene* pScene = importer.ReadFile(filename,
		aiProcess_Triangulate |
		aiProcess_ConvertToLeftHanded);

	if (pScene == nullptr)
		return false;

	this->directory_ = filename.substr(0, filename.find_last_of("/\\"));

	//this->dev_ = dev;
	this->hwnd_ = hwnd;

	processNode(pScene->mRootNode, pScene);
	//copyNode(RootNode, pScene->mRootNode, nullptr);
	return true;
}

void ModelLoader::Close() {
	for (auto& t : textures_loaded_)
		t.Release();
}

void ModelLoader::processNode(aiNode * node, const aiScene * scene) {
	for (UINT i = 0; i < node->mNumChildren; i++) {
		this->processNode(node->mChildren[i], scene);
	}
}


void ModelLoader::copyNode(innerModelNode* destnode, aiNode* srcnode, innerModelNode* parent)
{
	destnode->name = srcnode->mName.C_Str();
	destnode->transform = DirectX::XMMatrixTranspose( *(XMMATRIX*)&srcnode->mTransformation);
	//destnode->mMetaData = srcnode->mMetaData;
	destnode->numChildren = srcnode->mNumChildren;
	if (destnode->numChildren > 0) {
		destnode->Childrens = new innerModelNode * [destnode->numChildren];
		for (int i = 0; i < destnode->numChildren;++i) {
			destnode->Childrens[i] = new innerModelNode();
			copyNode(destnode->Childrens[i], srcnode->mChildren[i], destnode);
		}
	}
	else {
		destnode->numChildren = 0;
		destnode->Childrens = nullptr;
	}
	destnode->parent = parent;
	destnode->numMesh = srcnode->mNumMeshes;
	if (destnode->numMesh > 0) {
		destnode->Meshes = new unsigned int[destnode->numMesh];
		for (int i = 0; i < destnode->numMesh; ++i) {
			destnode->Meshes[i] = srcnode->mMeshes[i];
		}
	}
	else {
		destnode->numMesh = 0;
		destnode->Meshes = nullptr;
	}
}

std::vector<Texture> ModelLoader::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, const aiScene* scene)
{
	return std::vector<Texture>();
}

GPUResource ModelLoader::loadEmbeddedTexture(const aiTexture* embeddedTexture) {
	HRESULT hr;
	ID3D11ShaderResourceView *texture = nullptr;
	GPUResource tex;

	//if (embeddedTexture->mHeight != 0) {
	//	// Load an uncompressed ARGB8888 embedded texture
	//	D3D11_TEXTURE2D_DESC desc;
	//	desc.Width = embeddedTexture->mWidth;
	//	desc.Height = embeddedTexture->mHeight;
	//	desc.MipLevels = 1;
	//	desc.ArraySize = 1;
	//	desc.SampleDesc.Count = 1;
	//	desc.SampleDesc.Quality = 0;
	//	desc.Usage = D3D11_USAGE_DEFAULT;
	//	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	//	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	//	desc.CPUAccessFlags = 0;
	//	desc.MiscFlags = 0;

	//	D3D11_SUBRESOURCE_DATA subresourceData;
	//	subresourceData.pSysMem = embeddedTexture->pcData;
	//	subresourceData.SysMemPitch = embeddedTexture->mWidth * 4;
	//	subresourceData.SysMemSlicePitch = embeddedTexture->mWidth * embeddedTexture->mHeight * 4;

	//	ID3D11Texture2D *texture2D = nullptr;
	//	hr = dev_->CreateTexture2D(&desc, &subresourceData, &texture2D);
	//	if (FAILED(hr))
	//		MessageBox(hwnd_, L"CreateTexture2D failed!", L"Error!", MB_ICONERROR | MB_OK);

	//	hr = dev_->CreateShaderResourceView(texture2D, nullptr, &texture);
	//	if (FAILED(hr))
	//		MessageBox(hwnd_, L"CreateShaderResourceView failed!", L"Error!", MB_ICONERROR | MB_OK);

	//	return tex;
	//}

	// mHeight is 0, so try to load a compressed texture of mWidth bytes
	const size_t size = embeddedTexture->mWidth;

	ID3D12Resource* res = nullptr;
	tex.resource->QueryInterface<ID3D12Resource>(&res);
	std::unique_ptr<uint8_t[]> decodeD;
	D3D12_SUBRESOURCE_DATA sData;
	hr = LoadWICTextureFromMemory(dev_->pDevice, reinterpret_cast<const unsigned char*>(embeddedTexture->pcData), size, &res, decodeD, sData);
	if (FAILED(hr))
		MessageBox(hwnd_, L"Texture couldn't be created from memory!", L"Error!", MB_ICONERROR | MB_OK);

	return tex;
}
