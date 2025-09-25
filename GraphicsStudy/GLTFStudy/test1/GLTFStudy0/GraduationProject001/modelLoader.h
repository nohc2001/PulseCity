#include "gltf-loader.h" 
#include "main.h"

std::vector<Mesh> Mesh::LoadFromGLTF(ID3D12GraphicsCommandList* pCommandList, const std::string& filename)
{
    std::vector<GltfParser::Mesh> parsedMeshes;
    std::vector<GltfParser::Material> parsedMaterials;
    std::vector<GltfParser::Texture> parsedTextures;
    std::vector<GltfParser::Node> parsedNodes;
    std::vector<GltfParser::Skin> parsedSkins;
    std::vector<GltfParser::Animation> parsedAnimations;

    bool success = GltfParser::LoadGLTF(
        filename,
        parsedMeshes, parsedMaterials, parsedTextures,
        parsedNodes, parsedSkins, parsedAnimations
    );

    if (!success) {
        return {};
    }

    std::vector<Mesh> engineMeshes;
    for (const auto& gltfMesh : parsedMeshes)
    {
        std::vector<Mesh::Vertex> vertices;
        size_t vertexCount = gltfMesh.positions.size() / 3;
        vertices.reserve(vertexCount);

        for (size_t i = 0; i < vertexCount; ++i) {
            Mesh::Vertex v;

            v.position = XMFLOAT3(gltfMesh.positions[i * 3], gltfMesh.positions[i * 3 + 1], gltfMesh.positions[i * 3 + 2]);
            v.position.z *= -1.0f;

            if (!gltfMesh.normals.empty()) {
                v.normal = XMFLOAT3(gltfMesh.normals[i * 3], gltfMesh.normals[i * 3 + 1], gltfMesh.normals[i * 3 + 2]);
                v.normal.z *= -1.0f;
            }
            else {
                v.normal = XMFLOAT3(0, 1, 0);
            }

            v.color = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
            vertices.push_back(v);
        }

        std::vector<unsigned int> correctedIndices = gltfMesh.indices;
        for (size_t i = 0; i < correctedIndices.size(); i += 3) {
            std::swap(correctedIndices[i + 1], correctedIndices[i + 2]);
        }

        Mesh newEngineMesh;
        newEngineMesh.CreateGPUBuffers(pCommandList, vertices, correctedIndices);

        engineMeshes.push_back(newEngineMesh);
    }

    return engineMeshes;
}
