using UnityEngine;
using System.IO;
using System;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using static UnityEngine.UI.Image;
using static TMPro.SpriteAssetUtilities.TexturePacker_JsonArray;
using UnityEngine.Experimental.Rendering;
using System.Collections;
using System.Xml.Linq;
using System.Runtime.ConstrainedExecution;

public struct MeshMeta
{
    public string Name;
    public float ScaleFactor;
}

public struct Material_Data
{
    public struct CLR // 48 ~ 159
    {
        bool blending;
        float bumpscaling;
        Vector4 ambient;
        Vector4 diffuse;
        Vector4 emissive;
        Vector4 reflective;
        Vector4 specular;
        float transparent;
    };
    struct TextureIndex // 160 ~ 208
    {
        int Diffuse;
        int Metalic; // 0 : non metal / 1 : metal (binary texture)
        int AmbientOcculsion;
        int Emissive;
        int Bump; // in TBN Coord
        int Roughness;
        int Transparency;
        int LightMap;
        int Reflection;
        int Sheen;
        int ClearCoat;
        int Transmission;
        int Anisotropy;
    };
    public struct PipelineChanger
    {
        bool twosided;
        bool wireframe;
        short TextureSementicSelector;

        enum eTextureKind
        {
            Base,
            Specular_Metalic,
            Ambient,
            Emissive,
            Bump,
            Shineness,
            Opacity
        };
    };
    string name;
    CLR clr;
    TextureIndex ti;
    PipelineChanger pipelineC; // this change pipeline of materials.

    float metallicFactor;
    float roughnessFactor;
    float shininess;
    float opacity;
    float specularFactor;
    float clearcoat_factor;
    float clearcoat_roughnessFactor;
    int gltf_alphaMode;
    float gltf_alpha_cutoff;
};

public struct MaterialMeta
{
    public string Name;
    public Vector4 color;
    public float metalic;
    public float smoothness;
    public float normalFactor;
    public Vector2 Tiling;
    public Vector2 offset;   
    public Vector2 Tiling2;
    public Vector2 offset2;
    public bool renderMod_isTransparent;
    public bool emission;

    public int DiffuseTex;
    public int NormalTex;
    public int MetalicTex;
    public int OcclusionTex;
    public int RoughnessTex;
    public int EmissionTex;
    public int Diffuse2Tex;
    public int Normal2Tex;
}

public class ModelMeta
{
    public string Name;
    public GameObject Origin;
    public float ScaleFactor;

    public GameObject MapObject = null;

    int gameobjCount = 0;
    Dictionary<GameObject, int> ObjectToIDHashMap = null;
    List<GameObject> gameObjects = null;

    Dictionary<string, int> NameToIDHashMap = null;
    List<string> Names = null;
    int nameCount = 0;

    Dictionary<Mesh, int> MeshToIDHashMap = new Dictionary<Mesh, int> { };
    List<Mesh> Meshs = new List<Mesh>();

    Dictionary<Material, int> MaterialToIDHashMap = null;
    List<MaterialMeta> Materials = null;

    Dictionary<Texture, int> textureToIDHashMap = null;
    List<Texture> Textures = null;

    public string GetCommonSubString(string s1, string s2)
    {
        string str = new string("");
        for (int i = 0; i < s1.Length; ++i)
        {
            if (i >= s2.Length) break;
            if (s1[i] == s2[i])
            {
                str += s1[i];
            }
            else break;
        }
        return str;
    }
    public MaterialMeta Interpret_Material(Material mat)
    {
        MaterialMeta meta = new MaterialMeta();
        //render mode
        if (mat.GetFloat("_Mode") == 0) meta.renderMod_isTransparent = false;
        else meta.renderMod_isTransparent = true;

        meta.Name = mat.name;
        meta.color = mat.color;

        if (mat.HasProperty("_Metallic")) meta.metalic = mat.GetFloat("_Metallic");
        else meta.metalic = 0;

        // Attempt to get smoothness using "_Smoothness"
        if (mat.HasProperty("_Smoothness"))
            meta.smoothness = mat.GetFloat("_Smoothness");
        // If "_Smoothness" is not found, try "_Glossiness" (older versions or custom shaders might use this)
        else if (mat.HasProperty("_Glossiness"))
            meta.smoothness = mat.GetFloat("_Glossiness");
        else
            meta.smoothness = 0;

        if (mat.HasProperty("_BumpScale")) meta.normalFactor = mat.GetFloat("_BumpScale");
        else meta.normalFactor = 0;

        if (mat.IsKeywordEnabled("_EMISSION")) meta.emission = true;
        else meta.emission = false;

        meta.Tiling = mat.mainTextureScale;
        meta.offset = mat.mainTextureOffset;

        string secondaryTexturePropertyName = "_DetailAlbedoMap";
        meta.Tiling2 = mat.GetTextureScale(secondaryTexturePropertyName);
        meta.offset2 = mat.GetTextureOffset(secondaryTexturePropertyName);

        Texture mainTex = mat.GetTexture("_MainTex");
        Texture normalMap = mat.GetTexture("_BumpMap");
        meta.DiffuseTex = AddTexture(mainTex);
        meta.NormalTex = AddTexture(normalMap);
        meta.MetalicTex = AddTexture(mat.GetTexture("_MetallicGlossMap"));
        meta.OcclusionTex = AddTexture(mat.GetTexture("_OcclusionMap"));

        //roughness
        string commonName = GetCommonSubString(AssetDatabase.GetAssetPath(mainTex), AssetDatabase.GetAssetPath(normalMap));
        commonName += "Roughness";
        Texture roughness = (Texture)Resources.Load(commonName);
        meta.RoughnessTex = AddTexture(roughness);

        meta.EmissionTex = AddTexture(mat.GetTexture("_EmissionMap"));
        meta.Diffuse2Tex = AddTexture(mat.GetTexture("_DetailAlbedoMap"));
        meta.Normal2Tex = AddTexture(mat.GetTexture("_DetailNormalMap"));
        return meta;
    }

    public int AddName(string name)
    {
        if (name != null)
        {
            if (NameToIDHashMap.ContainsKey(name))
            {
                return NameToIDHashMap[name];
            }
            else
            {
                int up = Names.Count;
                Names.Add(name);
                NameToIDHashMap.Add(name, up);
                return up;
            }
        }
        return -1;
    }
    public int AddMesh(Mesh mesh)
    {
        if (mesh != null)
        {
            if (MeshToIDHashMap.ContainsKey(mesh))
            {
                return MeshToIDHashMap[mesh];
            }
            else
            {
                int up = Meshs.Count;
                Meshs.Add(mesh);
                MeshToIDHashMap.Add(mesh, up);
                return up;
            }
        }
        return -1;
    }

    public int AddTexture(Texture tex)
    {
        if (tex != null)
        {
            if (textureToIDHashMap.ContainsKey(tex))
            {
                return textureToIDHashMap[tex];
            }
            else
            {
                int up = Textures.Count;
                Textures.Add(tex);
                textureToIDHashMap.Add(tex, up);
                return up;
            }
        }
        return -1;
    }

    public int AddMaterial(Material mat, MaterialMeta meta)
    {
        if (mat != null)
        {
            if (MaterialToIDHashMap.ContainsKey(mat))
            {
                return MaterialToIDHashMap[mat];
            }
            else
            {
                int up = Materials.Count;
                Materials.Add(meta);
                MaterialToIDHashMap.Add(mat, up);
                return up;
            }
        }
        return -1;
    }

    public int AddGameObject(GameObject go)
    {
        if (go != null)
        {
            if (ObjectToIDHashMap.ContainsKey(go))
            {
                return ObjectToIDHashMap[go];
            }
            else {
                int up = gameObjects.Count;
                gameObjects.Add(go);
                ObjectToIDHashMap.Add(go, up);
                return up;
            }
        }
        return -1;
    }

    void CountingObject(GameObject go)
    {
        if (go != null)
        {
            gameobjCount += 1;

            int n = go.name.IndexOf('(');
            if (n > 0)
            {
                go.name = go.name.Substring(0, n-1);
            }

            AddName(go.name);
            AddGameObject(go);

            if (true) {
                MeshFilter meshFilter = go.GetComponent<MeshFilter>();
                if (meshFilter != null)
                {
                    AddMesh(meshFilter.sharedMesh);
                }

                MeshRenderer meshRenderer = go.GetComponent<MeshRenderer>();
                if (meshRenderer != null)
                {
                    for (int i = 0; i < meshRenderer.sharedMaterials.Length; ++i)
                    {
                        Material mat = meshRenderer.sharedMaterials[i];
                        MaterialMeta meta = Interpret_Material(mat);
                        AddMaterial(mat, meta);
                    }
                }

                for (int i = 0; i < go.transform.childCount; ++i)
                {
                    CountingObject(go.transform.GetChild(i).gameObject);
                }
            }
        }
    }

    void SaveObject(GameObject go, ref int index)
    {
        //if (go != null)
        //{
        //    gameObjects[index] = go;
        //    index += 1;

        //    if (true)
        //    {
        //        for (int i = 0; i < go.transform.childCount; ++i)
        //        {
        //            SaveObject(go.transform.GetChild(i).gameObject, ref index);
        //        }
        //    }
        //}
    }

    public void GetData(GameObject go)
    {
        Name = go.name;
        Origin = go;
        MeshToIDHashMap = new Dictionary<Mesh, int> { };
        Meshs = new List<Mesh>();
        Textures = new List<Texture>();
        textureToIDHashMap = new Dictionary<Texture, int>();
        MaterialToIDHashMap = new Dictionary<Material, int>();
        Materials = new List<MaterialMeta>();
        NameToIDHashMap = new Dictionary<string, int>();
        Names = new List<string>();
        gameObjects = new List<GameObject> { };
        ObjectToIDHashMap = new Dictionary<GameObject, int> { };
        CountingObject(go);
        int currindex = 0;
        SaveObject(MapObject, ref currindex);
    }

    public void BineryWrite(BinaryWriter writer)
    {
        writer.Write(nameCount);
        writer.Write(Meshs.Count);
        writer.Write(Textures.Count);
        writer.Write(Materials.Count);
        writer.Write(gameobjCount);
        for (int i = 0; i < Names.Count; ++i)
        {
            string name = (string)Names[i];
            writer.Write(name.Length);
            for (int k = 0; k < name.Length; ++k)
            {
                byte b = (byte)name[k];
                writer.Write(b);
            }
        }

        for (int i = 0; i < Meshs.Count; ++i)
        {
            Mesh mesh = Meshs[i];
            if (NameToIDHashMap.ContainsKey(mesh.name)) writer.Write(NameToIDHashMap[mesh.name]); // nameid
            else writer.Write(-1);

            writer.Write(mesh.bounds.center.x);
            writer.Write(mesh.bounds.center.y);
            writer.Write(mesh.bounds.center.z); // OBB Center

            writer.Write(mesh.bounds.extents.x);
            writer.Write(mesh.bounds.extents.y);
            writer.Write(mesh.bounds.extents.z); // OBB Extend
                                                 //not contain scale factor of mesh. >> Loading Mesh as name, OBB compare and it can Calculate ScaleFactor.
        }

        for (int i = 0; i < Textures.Count; ++i)
        {
            Texture texture = Textures[i];
            writer.Write(texture.name.Length);
            for (int k = 0; k < texture.name.Length; ++k)
            {
                byte b = (byte)texture.name[k];
                writer.Write(b);
            }
            //writer.Write(texture.name);
            writer.Write(texture.width);
            writer.Write(texture.height);
            GraphicsFormat format = texture.graphicsFormat;
            writer.Write((int)format);
        }

        for (int i = 0; i < Materials.Count; ++i)
        {
            MaterialMeta meta = Materials[i];
            writer.Write(meta.Name.Length);
            for(int k = 0; k < meta.Name.Length; ++k)
            {
                byte b = (byte)meta.Name[k];
                writer.Write(b);
            }
            //writer.Write(meta.Name);

            writer.Write(meta.color.x);
            writer.Write(meta.color.y);
            writer.Write(meta.color.z);
            writer.Write(meta.color.w);

            writer.Write(meta.metalic);

            writer.Write(meta.smoothness);

            writer.Write(meta.normalFactor);

            writer.Write(meta.Tiling.x);
            writer.Write(meta.Tiling.y);

            writer.Write(meta.offset.x);
            writer.Write(meta.offset.y);

            writer.Write(meta.Tiling2.x);
            writer.Write(meta.Tiling2.y);

            writer.Write(meta.offset2.x);
            writer.Write(meta.offset2.y);

            writer.Write(meta.renderMod_isTransparent);
            writer.Write(meta.emission);

            writer.Write(meta.DiffuseTex);
            writer.Write(meta.NormalTex);
            writer.Write(meta.MetalicTex);
            writer.Write(meta.OcclusionTex);
            writer.Write(meta.RoughnessTex);
            writer.Write(meta.EmissionTex);
            writer.Write(meta.Diffuse2Tex);
            writer.Write(meta.Normal2Tex);
        }

        for(int i=0;i< gameobjCount; ++i)
        {
            GameObject go = gameObjects[i];
            writer.Write(NameToIDHashMap[go.name]); // nameid

            writer.Write(go.transform.localPosition.x);
            writer.Write(go.transform.localPosition.y);
            writer.Write(go.transform.localPosition.z); //pos

            writer.Write(go.transform.localRotation.eulerAngles.x);
            writer.Write(go.transform.localRotation.eulerAngles.y);
            writer.Write(go.transform.localRotation.eulerAngles.z); // rot

            writer.Write(go.transform.localScale.x);
            writer.Write(go.transform.localScale.y);
            writer.Write(go.transform.localScale.z); // scale
            //transform

            MeshFilter meshFilter = go.GetComponent<MeshFilter>();
            if(meshFilter != null) writer.Write(MeshToIDHashMap[meshFilter.sharedMesh]);
            else writer.Write(-1);

            // mesh id
            MeshRenderer mr = go.GetComponent<MeshRenderer>();
            if(mr != null)
            {
                writer.Write(mr.sharedMaterials.Length); // num materials
                for (int k = 0; k < mr.sharedMaterials.Length; ++k)
                {
                    Material material = mr.sharedMaterials[k];
                    writer.Write(MaterialToIDHashMap[material]); //material id
                }
            }
            else
            {
                writer.Write(0);
            }
        }
    }

    public void SaveModel(string filepath, string dirpath)
    {
        using (FileStream fs = new FileStream(filepath, FileMode.Create))
        {
            using (BinaryWriter writer = new BinaryWriter(fs))
            {
                writer.Write(Meshs.Count);
                writer.Write(gameObjects.Count);
                writer.Write(Textures.Count);
                writer.Write(Materials.Count);

                for(int i = 0; i < Meshs.Count; ++i)
                {
                    Mesh mesh = Meshs[i];
                    Vector3 Start = mesh.bounds.center - mesh.bounds.extents;
                    Vector3 End = mesh.bounds.center + mesh.bounds.extents;
                    writer.Write(Start.x);
                    writer.Write(Start.y);
                    writer.Write(Start.z);
                    writer.Write(End.x);
                    writer.Write(End.x);
                    writer.Write(End.x);

                    Vector3[] vertices = mesh.vertices;
                    Vector2[] uvs = mesh.uv;
                    Vector3[] normals = mesh.normals;
                    Vector4[] tangents = mesh.tangents;

                    int[] triangles = mesh.triangles;

                    writer.Write(vertices.Length);
                    writer.Write(triangles.Length);

                    for (int k = 0; k < vertices.Length; ++k)
                    {
                        writer.Write(vertices[k].x);
                        writer.Write(vertices[k].y);
                        writer.Write(vertices[k].z);
                        writer.Write(uvs[k].x);
                        writer.Write(uvs[k].y);
                        writer.Write(normals[k].x);
                        writer.Write(normals[k].y);
                        writer.Write(normals[k].z);
                        writer.Write(tangents[k].x);
                        writer.Write(tangents[k].y);
                        writer.Write(tangents[k].z);
                        Vector3 binormal = Vector3.Cross(normals[k], tangents[k]);
                        writer.Write(binormal.x);
                        writer.Write(binormal.y);
                        writer.Write(binormal.z);
                    }

                    for (int k = 0; k < triangles.Length; ++k)
                    {
                        writer.Write(triangles[k]);
                    }
                }

                for (int i = 0; i < gameObjects.Count; ++i)
                {
                    writer.Write(gameObjects[i].name.Length);
                    for(int k = 0;k< gameObjects[i].name.Length; ++k)
                    {
                        byte c = (byte)gameObjects[i].name[k];
                        writer.Write(c);
                    }

                    //world matrix
                    writer.Write(gameObjects[i].transform.localPosition.x);
                    writer.Write(gameObjects[i].transform.localPosition.y);
                    writer.Write(gameObjects[i].transform.localPosition.z);

                    writer.Write(gameObjects[i].transform.localRotation.eulerAngles.x);
                    writer.Write(gameObjects[i].transform.localRotation.eulerAngles.y);
                    writer.Write(gameObjects[i].transform.localRotation.eulerAngles.z);

                    writer.Write(gameObjects[i].transform.localScale.x);
                    writer.Write(gameObjects[i].transform.localScale.y);
                    writer.Write(gameObjects[i].transform.localScale.z);

                    if (ObjectToIDHashMap.ContainsKey(gameObjects[i].transform.parent.gameObject))
                    {
                        int parentId = ObjectToIDHashMap[gameObjects[i].transform.parent.gameObject];
                        writer.Write(parentId);
                    }
                    else writer.Write(-1);

                    writer.Write(gameObjects[i].transform.childCount);

                    //num mesh
                    MeshFilter meshFilter = gameObjects[i].GetComponent<MeshFilter>();
                    int numMesh = 1;
                    if (meshFilter != null && meshFilter.sharedMesh != null) writer.Write(numMesh);
                    else
                    {
                        numMesh = 0;
                        writer.Write(0);
                    }

                    for (int k = 0; k < gameObjects[i].transform.childCount; ++k)
                        {
                            writer.Write(ObjectToIDHashMap[gameObjects[i].transform.GetChild(k).gameObject]);
                        }

                    for(int k = 0; k < numMesh; ++k)
                    {
                        MeshRenderer renderer = gameObjects[i].GetComponent<MeshRenderer>();
                        writer.Write(MeshToIDHashMap[meshFilter.sharedMesh]);

                        writer.Write(renderer.sharedMaterials.Length);
                        for(int u=0;u< renderer.sharedMaterials.Length; ++u)
                        {
                            writer.Write(MaterialToIDHashMap[renderer.sharedMaterials[u]]);
                        }
                    }
                }

                for (int i = 0; i < Textures.Count; ++i)
                {
                    string texpath = dirpath;
                    texpath += "/";
                    texpath += Name;
                    texpath += i.ToString();
                    texpath += ".tex";
                    using (FileStream fs2 = new FileStream(texpath, FileMode.Create))
                    {
                        using (BinaryWriter writer2 = new BinaryWriter(fs2))
                        {
                            Texture texture = Textures[i];
                            Texture2D tex2D = texture as Texture2D;
                            Color32[] pixels = tex2D.GetPixels32();

                            writer2.Write(texture.width);
                            writer2.Write(texture.height);
                            foreach (Color32 c in pixels)
                            {
                                writer2.Write(c.r);
                                writer2.Write(c.g);
                                writer2.Write(c.b);
                                writer2.Write(c.a);
                            }
                        }
                    }
                }

                for (int i = 0; i < Materials.Count; ++i)
                {
                    MaterialMeta meta = Materials[i];
                    writer.Write(meta.Name.Length);
                    for (int k = 0; k < meta.Name.Length; ++k)
                    {
                        byte c = (byte)meta.Name[k];
                        writer.Write(c);
                    }

                    writer.Write(meta.color.x);
                    writer.Write(meta.color.y);
                    writer.Write(meta.color.z);
                    writer.Write(meta.color.w);

                    writer.Write(meta.metalic);

                    writer.Write(meta.smoothness);

                    writer.Write(meta.normalFactor);

                    writer.Write(meta.Tiling.x);
                    writer.Write(meta.Tiling.y);

                    writer.Write(meta.offset.x);
                    writer.Write(meta.offset.y);

                    writer.Write(meta.Tiling2.x);
                    writer.Write(meta.Tiling2.y);

                    writer.Write(meta.offset2.x);
                    writer.Write(meta.offset2.y);

                    writer.Write(meta.renderMod_isTransparent);
                    writer.Write(meta.emission);

                    writer.Write(meta.DiffuseTex);
                    writer.Write(meta.NormalTex);
                    writer.Write(meta.MetalicTex);
                    writer.Write(meta.OcclusionTex);
                    writer.Write(meta.RoughnessTex);
                    writer.Write(meta.EmissionTex);
                    writer.Write(meta.Diffuse2Tex);
                    writer.Write(meta.Normal2Tex);
                }
            }
        }
    }
}

public class Unity_BinaryMapExporter : MonoBehaviour
{
    public string mapName = "data";
    public GameObject MapObject = null;
    
    int gameobjCount = 0;
    Dictionary<GameObject, int> ObjectToIDHashMap = null;
    List<GameObject> gameObjects = null;

    Dictionary<string, int> NameToIDHashMap = null;
    List<string> Names = null;
    int nameCount = 0;

    Dictionary<Mesh, int> MeshToIDHashMap = new Dictionary<Mesh, int> { };
    List<Mesh> Meshs = new List<Mesh>();

    HashSet<string> ModelNameSet = null;
    List<GameObject> models;

    Dictionary<Material, int> MaterialToIDHashMap = null;
    List<MaterialMeta> Materials = null;

    Dictionary<Texture, int> textureToIDHashMap = null;
    List<Texture> Textures = null;

    Dictionary<string, int> ModelToIDHashMap = null;
    List<ModelMeta> Models = null;

    public string GetCommonSubString(string s1, string s2)
    {
        string str = new string("");
        for(int i = 0; i < s1.Length; ++i)
        {
            if (i >= s2.Length) break;
            if (s1[i] == s2[i])
            {
                str += s1[i];
            }
            else break;
        }
        return str;
    }

    public MaterialMeta Interpret_Material(Material mat)
    {
        MaterialMeta meta = new MaterialMeta();
        //render mode
        if (mat.GetFloat("_Mode") == 0) meta.renderMod_isTransparent = false;
        else meta.renderMod_isTransparent = true;

        meta.Name = mat.name;
        meta.color = mat.color;

        if (mat.HasProperty("_Metallic")) meta.metalic = mat.GetFloat("_Metallic");
        else meta.metalic = 0;

        // Attempt to get smoothness using "_Smoothness"
        if (mat.HasProperty("_Smoothness"))
            meta.smoothness = mat.GetFloat("_Smoothness");
        // If "_Smoothness" is not found, try "_Glossiness" (older versions or custom shaders might use this)
        else if (mat.HasProperty("_Glossiness"))
            meta.smoothness = mat.GetFloat("_Glossiness");
        else
            meta.smoothness = 0;

        if (mat.HasProperty("_BumpScale")) meta.normalFactor = mat.GetFloat("_BumpScale");
        else meta.normalFactor = 0;

        if (mat.IsKeywordEnabled("_EMISSION")) meta.emission = true;
        else meta.emission = false;

        meta.Tiling = mat.mainTextureScale;
        meta.offset = mat.mainTextureOffset;

        string secondaryTexturePropertyName = "_DetailAlbedoMap";
        meta.Tiling2 = mat.GetTextureScale(secondaryTexturePropertyName);
        meta.offset2 = mat.GetTextureOffset(secondaryTexturePropertyName);

        Texture mainTex = mat.GetTexture("_MainTex");
        Texture normalMap = mat.GetTexture("_BumpMap");
        meta.DiffuseTex = AddTexture(mainTex);
        meta.NormalTex = AddTexture(normalMap);
        meta.MetalicTex = AddTexture(mat.GetTexture("_MetallicGlossMap"));
        meta.OcclusionTex = AddTexture(mat.GetTexture("_OcclusionMap"));

        //roughness
        string commonName = GetCommonSubString(AssetDatabase.GetAssetPath(mainTex), AssetDatabase.GetAssetPath(normalMap));
        commonName += "Roughness";
        Texture roughness = (Texture)Resources.Load(commonName);
        meta.RoughnessTex = AddTexture(roughness);
        
        meta.EmissionTex = AddTexture(mat.GetTexture("_EmissionMap"));
        meta.Diffuse2Tex = AddTexture(mat.GetTexture("_DetailAlbedoMap"));
        meta.Normal2Tex = AddTexture(mat.GetTexture("_DetailNormalMap"));
        return meta;
    }

    public int AddName(string name)
    {
        if (name != null)
        {
            if (NameToIDHashMap.ContainsKey(name))
            {
                return NameToIDHashMap[name];
            }
            else
            {
                int up = Names.Count;
                Names.Add(name);
                NameToIDHashMap.Add(name, up);
                return up;
            }
        }
        return -1;
    }

    public int AddGameObject(GameObject go)
    {
        if (go != null)
        {
            if (ObjectToIDHashMap.ContainsKey(go))
            {
                return ObjectToIDHashMap[go];
            }
            else
            {
                int up = gameObjects.Count;
                gameObjects.Add(go);
                ObjectToIDHashMap.Add(go, up);
                return up;
            }
        }
        return -1;
    }

    public int AddMesh(Mesh mesh)
    {
        if (mesh != null)
        {
            if (MeshToIDHashMap.ContainsKey(mesh))
            {
                return MeshToIDHashMap[mesh];
            }
            else
            {
                int up = Meshs.Count;
                Meshs.Add(mesh);
                MeshToIDHashMap.Add(mesh, up);
                return up;
            }
        }
        return -1;
    }

    public int AddModel(GameObject model)
    {
        if (model != null)
        {
            if (ModelToIDHashMap.ContainsKey(model.name))
            {
                return ModelToIDHashMap[model.name];
            }
            else
            {
                int up = Models.Count;
                ModelMeta meta = new ModelMeta();
                meta.GetData(model);
                Models.Add(meta);
                ModelToIDHashMap.Add(model.name, up);
                return up;
            }
        }
        return -1;
    }

    public int AddTexture(Texture tex)
    {
        if (tex != null)
        {
            if (textureToIDHashMap.ContainsKey(tex))
            {
                return textureToIDHashMap[tex];
            }
            else
            {
                int up = Textures.Count;
                Textures.Add(tex);
                textureToIDHashMap.Add(tex, up);
                return up;
            }
        }
        return -1;
    }

    public int AddMaterial(Material mat, MaterialMeta meta)
    {
        if (mat != null)
        {
            if (MaterialToIDHashMap.ContainsKey(mat))
            {
                return MaterialToIDHashMap[mat];
            }
            else
            {
                int up = Materials.Count;
                Materials.Add(meta);
                MaterialToIDHashMap.Add(mat, up);
                return up;
            }
        }
        return -1;
    }

    void CountingObject(GameObject go)
    {
        if(go != null)
        {
            gameobjCount += 1;
            
            int n = go.name.IndexOf('(');
            if(n > 0)
            {
                // 괄호 부분 지우기
                go.name = go.name.Substring(0, n-1);
            }

            AddName(go.name);
            AddGameObject(go);

            if (go.tag == "Prefab")
            {
                //Model
                AddModel(go);
            }
            else
            {
                MeshFilter meshFilter = go.GetComponent<MeshFilter>();
                if (meshFilter != null)
                {
                    AddMesh(meshFilter.sharedMesh);
                    AddName(meshFilter.sharedMesh.name);
                }

                MeshRenderer meshRenderer = go.GetComponent<MeshRenderer>();
                if (meshRenderer != null) {
                    for(int i=0;i< meshRenderer.sharedMaterials.Length; ++i)
                    {
                        Material mat = meshRenderer.sharedMaterials[i];
                        MaterialMeta meta = Interpret_Material(mat);
                        AddMaterial(mat, meta);
                    }
                }

                for (int i = 0; i < go.transform.childCount; ++i)
                {
                    CountingObject(go.transform.GetChild(i).gameObject);
                }
            }
        }
    }

    void SaveObject(GameObject go, ref int index)
    {
        if (go != null)
        {
            gameObjects[index] = go;
            index += 1;

            if (go.tag != "Prefab")
            {
                for (int i = 0; i < go.transform.childCount; ++i)
                {
                    SaveObject(go.transform.GetChild(i).gameObject, ref index);
                }
            }
        }
    }

    // Start is called once before the first execution of Update after the MonoBehaviour is created
    void Start()
    {
        MeshToIDHashMap = new Dictionary<Mesh, int> { };
        Meshs = new List<Mesh>();
        ModelToIDHashMap = new Dictionary<string, int>();
        Models = new List<ModelMeta>();
        Textures = new List<Texture>();
        textureToIDHashMap = new Dictionary<Texture, int>();
        ModelNameSet = new HashSet<string> ();
        //MaterialSet = new HashSet<Material>();
        MaterialToIDHashMap = new Dictionary<Material, int> ();
        Materials = new List<MaterialMeta> ();
        NameToIDHashMap = new Dictionary<string, int> ();
        Names = new List<string> ();
        gameObjects = new List<GameObject> { };
        ObjectToIDHashMap = new Dictionary<GameObject, int> { };
        CountingObject(MapObject);
        int currindex = 0;
        SaveObject(MapObject, ref currindex);
        // map format
        /*
         * int NameCount;
         * int meshCount;
         * int textureCount;
         * int materialCount;
         * int modelCount;
         * int objectCount
         * 
         * array<string> Names;
         * string = [int length, array<char> str]
         * 
         * array<Mesh>
         * Mesh = [nameid, scaleFactor]
         * 
         * array<Texture>
         * Texture = [ textureName=[len, array<char>], resolusion=[width, height], pixelFormat ]
         * 
         * array<Material>
         * Material = [nameid, TextureID (Color, Metalic, Normal, HeightMap(ifExist), Occulsion(Ambient), RoughnessMap(if roughness Map Exist in SameNameTexture_Roughness)), 
         *  float4 color, float metalicFactor, float smoothnessFactor, RanderMod=[Opacue or Transparent]]
         * 
         * array<Model> models;
         * Model = [nameid, 
         * meshCount, array<Mesh>
         * textureCount, array<texture>
         * materialCount, array<Material>
         * objectCount, array<Object>
         * ]
         * 
         * Object = [nameid, pos(x, y, z), rot(x, y, z), scale(x, y, z), ModelID]
         * array<Object> Objects;
         */

        string folderPath = mapName;
        if (!Directory.Exists(folderPath))
        {
            Directory.CreateDirectory(folderPath);
            Debug.Log("디렉토리 생성 완료: " + folderPath);
        }
        else
        {
            Debug.Log("이미 디렉토리가 존재합니다: " + folderPath);
        }

        // Map MetaData
        string filepath = mapName;
        filepath += "/";
        filepath += mapName;
        filepath += ".map";
        using (FileStream fs = new FileStream(filepath, FileMode.Create))
        {
            using (BinaryWriter writer = new BinaryWriter(fs))
            {
                writer.Write(Names.Count);
                writer.Write(Meshs.Count);
                writer.Write(Textures.Count);
                writer.Write(Materials.Count);
                writer.Write(Models.Count);
                writer.Write(gameobjCount);

                for (int i = 0; i < Names.Count; ++i)
                {
                    string name = (string)Names[i];
                    writer.Write(name.Length);
                    for (int k = 0; k < name.Length; ++k)
                    {
                        byte c = (byte)name[k];
                        writer.Write(c);
                    }
                }

                for(int i = 0; i < Meshs.Count; ++i)
                {
                    Mesh mesh = Meshs[i];
                    writer.Write(NameToIDHashMap[mesh.name]); // nameid
                    
                    writer.Write(mesh.bounds.center.x);
                    writer.Write(mesh.bounds.center.y);
                    writer.Write(mesh.bounds.center.z); // OBB Center

                    writer.Write(mesh.bounds.extents.x);
                    writer.Write(mesh.bounds.extents.y);
                    writer.Write(mesh.bounds.extents.z); // OBB Extend
                    //not contain scale factor of mesh. >> Loading Mesh as name, OBB compare and it can Calculate ScaleFactor.
                }

                for(int i = 0; i < Textures.Count; ++i)
                {
                    Texture texture = Textures[i];
                    writer.Write(texture.name.Length);
                    for (int k = 0; k < texture.name.Length; ++k)
                    {
                        byte c = (byte)texture.name[k];
                        writer.Write(c);
                    }
                    writer.Write(texture.width);
                    writer.Write(texture.height);
                    GraphicsFormat format = texture.graphicsFormat;
                    writer.Write((int)format);
                }

                for (int i = 0; i < Materials.Count; ++i)
                {
                    MaterialMeta meta = Materials[i];
                    writer.Write(meta.Name.Length);
                    for (int k = 0; k < meta.Name.Length; ++k)
                    {
                        byte c = (byte)meta.Name[k];
                        writer.Write(c);
                    }

                    writer.Write(meta.color.x);
                    writer.Write(meta.color.y);
                    writer.Write(meta.color.z);
                    writer.Write(meta.color.w);

                    writer.Write(meta.metalic);

                    writer.Write(meta.smoothness);

                    writer.Write(meta.normalFactor);
                    
                    writer.Write(meta.Tiling.x);
                    writer.Write(meta.Tiling.y);

                    writer.Write(meta.offset.x);
                    writer.Write(meta.offset.y);

                    writer.Write(meta.Tiling2.x);
                    writer.Write(meta.Tiling2.y);

                    writer.Write(meta.offset2.x);
                    writer.Write(meta.offset2.y);

                    writer.Write(meta.renderMod_isTransparent);
                    writer.Write(meta.emission);

                    writer.Write(meta.DiffuseTex);
                    writer.Write(meta.NormalTex);
                    writer.Write(meta.MetalicTex);
                    writer.Write(meta.OcclusionTex);
                    writer.Write(meta.RoughnessTex);
                    writer.Write(meta.EmissionTex);
                    writer.Write(meta.Diffuse2Tex);
                    writer.Write(meta.Normal2Tex);
                }

                for(int i = 0; i < Models.Count; ++i)
                {
                    ModelMeta model = Models[i];
                    writer.Write(model.Name.Length);
                    writer.Write(model.Name.ToCharArray());
                    //model.BineryWrite(writer);
                }

                for (int i = 0; i < gameobjCount; ++i)
                {
                    GameObject go = gameObjects[i];
                    writer.Write(NameToIDHashMap[go.name]); // nameid

                    writer.Write(go.transform.localPosition.x);
                    writer.Write(go.transform.localPosition.y);
                    writer.Write(go.transform.localPosition.z); //pos

                    writer.Write(go.transform.localRotation.eulerAngles.x);
                    writer.Write(go.transform.localRotation.eulerAngles.y);
                    writer.Write(go.transform.localRotation.eulerAngles.z); // rot

                    writer.Write(go.transform.localScale.x);
                    writer.Write(go.transform.localScale.y);
                    writer.Write(go.transform.localScale.z); // scale

                    bool isMesh = true;
                    if(go.tag == "Prefab")
                    {
                        isMesh = false;
                    }
                    writer.Write(isMesh);

                    if (isMesh)
                    {
                        MeshFilter meshFilter = go.GetComponent<MeshFilter>();
                        MeshRenderer mr = go.GetComponent<MeshRenderer>();
                        if (meshFilter != null && mr != null)
                        {
                            writer.Write(MeshToIDHashMap[go.GetComponent<MeshFilter>().sharedMesh]); // mesh id
                            writer.Write(mr.sharedMaterials.Length); // num materials
                            for (int k = 0; k < mr.sharedMaterials.Length; ++k)
                            {
                                Material material = mr.sharedMaterials[k];
                                writer.Write(MaterialToIDHashMap[material]); //material id
                            }
                        }
                        else
                        {
                            writer.Write(-1);
                            writer.Write(0);
                        }
                    }
                    else
                    {
                        //Model
                        if (ModelToIDHashMap.ContainsKey(go.name))
                        {
                            writer.Write(ModelToIDHashMap[go.name]);
                        }
                        else writer.Write(-1);
                    }

                    if (go.tag != "Prefab")
                    {
                        writer.Write(go.transform.childCount);
                        for (int k = 0; k < go.transform.childCount; ++k)
                        {
                            GameObject child = go.transform.GetChild(k).gameObject;
                            writer.Write(ObjectToIDHashMap[child]);
                        }
                    }
                }
            }
        }

        string folderPath2 = mapName;
        folderPath2 += "/Mesh";
        if (!Directory.Exists(folderPath2))
        {
            Directory.CreateDirectory(folderPath2);
            Debug.Log("디렉토리 생성 완료: " + folderPath2);
        }
        else
        {
            Debug.Log("이미 디렉토리가 존재합니다: " + folderPath2);
        }

        for (int i = 0; i < Meshs.Count; ++i)
        {
            Mesh mesh = Meshs[i];
            string filename = mapName;
            filename += "/Mesh/";
            filename += mesh.name;
            filename += ".mesh";
            using (FileStream fs = new FileStream(filename, FileMode.Create))
            {
                using (BinaryWriter writer = new BinaryWriter(fs))
                {
                    Vector3[] vertices = mesh.vertices;
                    Vector2[] uvs = mesh.uv;
                    Vector3[] normals = mesh.normals;
                    Vector4[] tangents = mesh.tangents;

                    int[] triangles = mesh.triangles;

                    writer.Write(vertices.Length);
                    writer.Write(triangles.Length);

                    for (int k=0;k< vertices.Length;++k)
                    {
                        writer.Write(vertices[k].x);
                        writer.Write(vertices[k].y);
                        writer.Write(vertices[k].z);
                        writer.Write(uvs[k].x);
                        writer.Write(uvs[k].y);
                        writer.Write(normals[k].x);
                        writer.Write(normals[k].y);
                        writer.Write(normals[k].z);
                        writer.Write(tangents[k].x);
                        writer.Write(tangents[k].y);
                        writer.Write(tangents[k].z);
                        writer.Write(tangents[k].w); // w는 binormal 방향
                    }

                    for (int k=0;k<triangles.Length;++k)
                    {
                        writer.Write(triangles[k]);
                    }
                }
            }
        }

        string folderPath3 = mapName;
        folderPath3 += "/Model";
        if (!Directory.Exists(folderPath3))
        {
            Directory.CreateDirectory(folderPath3);
            Debug.Log("디렉토리 생성 완료: " + folderPath3);
        }
        else
        {
            Debug.Log("이미 디렉토리가 존재합니다: " + folderPath3);
        }

        for(int i = 0; i < Models.Count; ++i)
        {
            string modelpath = folderPath3;
            modelpath += "/";
            modelpath += Models[i].Name;
            modelpath += ".model";
            Models[i].SaveModel(modelpath, folderPath3);
        }

        string folderPath4 = mapName;
        folderPath4 += "/Texture";
        if (!Directory.Exists(folderPath4))
        {
            Directory.CreateDirectory(folderPath4);
            Debug.Log("디렉토리 생성 완료: " + folderPath4);
        }
        else
        {
            Debug.Log("이미 디렉토리가 존재합니다: " + folderPath4);
        }

        for (int i = 0; i < Textures.Count; ++i)
        {
            Texture texture = Textures[i];
            Texture2D texture2D = texture as Texture2D;
            if (texture2D != null)
            {
                string texpath = folderPath4;
                texpath += "/";
                texpath += texture.name;
                texpath += ".tex";
                using (FileStream fs = new FileStream(texpath, FileMode.Create))
                {
                    using (BinaryWriter writer = new BinaryWriter(fs))
                    {
                        Color32[] pixels = texture2D.GetPixels32();

                        writer.Write(texture.width);
                        writer.Write(texture.height);
                        foreach (Color32 c in pixels)
                        {
                            writer.Write(c.r);
                            writer.Write(c.g);
                            writer.Write(c.b);
                            writer.Write(c.a);
                        }
                    }
                }
            }
        }
    }

    // Update is called once per frame
    void Update()
    {
        
    }
}
