#ifndef MODEL_LOADER_H
#define MODEL_LOADER_H

#include <vector>
#include <d3d11_1.h>
#include <DirectXMath.h>
#include <string>

#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>

#include "Mesh.h"
#include "../WICTextureLoader12.h"

using namespace DirectX;

class ModelLoader
{
public:
	ModelLoader();
	~ModelLoader();

	bool Load(HWND hwnd, ID3D11Device* dev, ID3D11DeviceContext* devcon, std::string filename);

	void Close();

	GlobalDevice* dev_;
	std::string directory_;
	std::vector<Texture> textures_loaded_;
	HWND hwnd_;
	//ModelNode* RootNode = nullptr;

	void processNode(aiNode* node, const aiScene* scene);
	void copyNode(void* destnode, aiNode* srcnode, void* parent, int index);
	std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, const aiScene* scene);
	GPUResource loadEmbeddedTexture(const aiTexture* embeddedTexture);
};

#endif // !MODEL_LOADER_H

