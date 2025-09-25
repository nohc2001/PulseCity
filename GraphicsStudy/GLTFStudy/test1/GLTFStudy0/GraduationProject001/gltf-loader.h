#ifndef GLTF_LOADER_H_
#define GLTF_LOADER_H_

#include <string>
#include <vector>
#include <map>

namespace GltfParser {

    struct Node {
        int parent = -1;
        std::vector<int> children;
        std::string name;

        std::vector<double> translation = { 0.0, 0.0, 0.0 };
        std::vector<double> rotation = { 0.0, 0.0, 0.0, 1.0 }; // Quaternion
        std::vector<double> scale = { 1.0, 1.0, 1.0 };
        std::vector<double> matrix = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };

        int mesh = -1;
        int skin = -1;
    };

    struct Material {
        std::string name;

    };

    struct Texture {
        std::string name;
        int width = 0;
        int height = 0;
        int components = 0;
        std::vector<unsigned char> image;
    };

    struct Skin {
        std::string name;
        int skeletonRootNode = -1;
        std::vector<int> joints;
        std::vector<float> inverseBindMatrices;
    };

    struct AnimationSampler {
        std::string interpolation;
        std::vector<float> inputTimes;
        std::vector<float> outputValues;
    };

    struct AnimationChannel {
        int samplerIndex;
        int targetNode;
        std::string targetPath;
    };

    struct Animation {
        std::string name;
        std::vector<AnimationSampler> samplers;
        std::vector<AnimationChannel> channels;
    };

    struct Mesh {
        std::string name;

        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<float> texcoords;

        std::vector<unsigned short> joints;
        std::vector<float> weights;

        std::vector<unsigned int> indices;

        int material_id = -1;
    };


    ///
    /// Loads all relevant data from a glTF 2.0 file into engine-friendly structures.
    ///
    bool LoadGLTF(const std::string& filename,
        std::vector<Mesh>& meshes,
        std::vector<Material>& materials,
        std::vector<Texture>& textures,
        std::vector<Node>& nodes,
        std::vector<Skin>& skins,
        std::vector<Animation>& animations);

} // namespace GltfParser

#endif // GLTF_LOADER_H_