#include "gltf-loader.h"

#include <iostream>
#include <vector>
#include <string>

// --- TinyGLTF 라이브러리 ---
#define STB_IMAGE_IMPLEMENTATION

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"


namespace GltfParser { 

    template <typename T_out, typename T_in>
    void getAccessorData(const tinygltf::Model& model, int accessorIndex, std::vector<T_out>& out_data) {
        if (accessorIndex < 0) return;
        const auto& accessor = model.accessors[accessorIndex];
        const auto& bufferView = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[bufferView.buffer];
        const size_t numComponents = tinygltf::GetNumComponentsInType(accessor.type);

        std::vector<T_in> tempData;
        tempData.resize(accessor.count * numComponents);
        const unsigned char* data_ptr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
        size_t byte_stride = accessor.ByteStride(bufferView);
        if (byte_stride == 0) {
            byte_stride = tinygltf::GetComponentSizeInBytes(accessor.componentType) * numComponents;
        }

        for (size_t i = 0; i < accessor.count; ++i) {
            memcpy(reinterpret_cast<unsigned char*>(tempData.data()) + i * numComponents * sizeof(T_in),
                data_ptr + i * byte_stride,
                numComponents * tinygltf::GetComponentSizeInBytes(accessor.componentType));
        }

        out_data.resize(tempData.size());
        std::copy(tempData.begin(), tempData.end(), out_data.begin());
    }


    static std::string GetFilePathExtension(const std::string& FileName) {
        if (FileName.find_last_of(".") != std::string::npos)
            return FileName.substr(FileName.find_last_of(".") + 1);
        return "";
    }

    ///
    /// Loads all relevant data from a glTF 2.0 file into engine-friendly structures.
    ///
    bool LoadGLTF(const std::string& filename,
        std::vector<Mesh>& meshes,
        std::vector<Material>& materials,
        std::vector<Texture>& textures,
        std::vector<Node>& nodes,
        std::vector<Skin>& skins,
        std::vector<Animation>& animations) {

        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        bool ret = GetFilePathExtension(filename) == "glb"
            ? loader.LoadBinaryFromFile(&model, &err, &warn, filename)
            : loader.LoadASCIIFromFile(&model, &err, &warn, filename);

        if (!warn.empty()) std::cout << "glTF WARN: " << warn << std::endl;
        if (!err.empty()) std::cerr << "glTF ERR: " << err << std::endl;
        if (!ret) {
            std::cerr << "Failed to load glTF file: " << filename << std::endl;
            return false;
        }

        // Node Data 
        nodes.reserve(model.nodes.size());
        for (const auto& gltf_node : model.nodes) {
            Node n;
            n.name = gltf_node.name;
            n.children = gltf_node.children;
            n.mesh = gltf_node.mesh;
            n.skin = gltf_node.skin;
            if (!gltf_node.translation.empty()) n.translation = gltf_node.translation;
            if (!gltf_node.rotation.empty()) n.rotation = gltf_node.rotation;
            if (!gltf_node.scale.empty()) n.scale = gltf_node.scale;
            if (!gltf_node.matrix.empty()) n.matrix = gltf_node.matrix;
            nodes.push_back(n);
        }
        for (size_t i = 0; i < nodes.size(); ++i) {
            for (int child_idx : nodes[i].children) {
                nodes[child_idx].parent = i;
            }
        }

        // Skin
        skins.reserve(model.skins.size());
        for (const auto& gltf_skin : model.skins) {
            Skin s;
            s.name = gltf_skin.name;
            s.skeletonRootNode = gltf_skin.skeleton;
            s.joints = gltf_skin.joints;
            getAccessorData<float, float>(model, gltf_skin.inverseBindMatrices, s.inverseBindMatrices);
            skins.push_back(s);
        }

        // Animation
        animations.reserve(model.animations.size());
        for (const auto& gltf_anim : model.animations) {
            Animation a;
            a.name = gltf_anim.name;
            for (const auto& gltf_sampler : gltf_anim.samplers) {
                AnimationSampler s;
                s.interpolation = gltf_sampler.interpolation;
                getAccessorData<float, float>(model, gltf_sampler.input, s.inputTimes);
                getAccessorData<float, float>(model, gltf_sampler.output, s.outputValues);
                a.samplers.push_back(s);
            }
            for (const auto& gltf_channel : gltf_anim.channels) {
                AnimationChannel c;
                c.samplerIndex = gltf_channel.sampler;
                c.targetNode = gltf_channel.target_node;
                c.targetPath = gltf_channel.target_path;
                a.channels.push_back(c);
            }
            animations.push_back(a);
        }

        // mesh
        for (const auto& gltfMesh : model.meshes) {
            for (const auto& primitive : gltfMesh.primitives) {
                Mesh newMesh;
                newMesh.name = gltfMesh.name;
                newMesh.material_id = primitive.material;

                // index data
                const auto& indexAccessor = model.accessors[primitive.indices];
                if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    getAccessorData<unsigned int, unsigned int>(model, primitive.indices, newMesh.indices);
                }
                else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    getAccessorData<unsigned int, unsigned short>(model, primitive.indices, newMesh.indices);
                }
                else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    getAccessorData<unsigned int, unsigned char>(model, primitive.indices, newMesh.indices);
                }

                // Attribute data
                for (const auto& attribute : primitive.attributes) {
                    if (attribute.first == "POSITION") {
                        getAccessorData<float, float>(model, attribute.second, newMesh.positions);
                    }
                    else if (attribute.first == "NORMAL") {
                        getAccessorData<float, float>(model, attribute.second, newMesh.normals);
                    }
                    else if (attribute.first == "TEXCOORD_0") {
                        getAccessorData<float, float>(model, attribute.second, newMesh.texcoords);
                    }
                    else if (attribute.first == "JOINTS_0") {
                        getAccessorData<unsigned short, unsigned short>(model, attribute.second, newMesh.joints);
                    }
                    else if (attribute.first == "WEIGHTS_0") {
                        getAccessorData<float, float>(model, attribute.second, newMesh.weights);
                    }
                }
                meshes.push_back(newMesh);
            }
        }

        // texture
        textures.reserve(model.textures.size());
        for (const auto& gltfTexture : model.textures) {
            Texture newTexture;
            newTexture.name = gltfTexture.name;

            const auto& image = model.images[gltfTexture.source];
            newTexture.width = image.width;
            newTexture.height = image.height;
            newTexture.components = image.component;
            newTexture.image = image.image;


            textures.push_back(newTexture);
        }

        return true;
    }

} // namespace GltfParser