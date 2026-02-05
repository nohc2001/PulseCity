using UnityEngine;
using UnityEditor;

public class ReadWriteSetter
{
    [MenuItem("Tools/Enable ReadWrite On All Assets")]
    static void EnableReadWriteOnAll()
    {
        string[] guids = AssetDatabase.FindAssets("t:Object"); // 모든 에셋 검색
        foreach (string guid in guids)
        {
            string path = AssetDatabase.GUIDToAssetPath(guid);
            AssetImporter importer = AssetImporter.GetAtPath(path);

            if (importer is ModelImporter modelImporter)
            {
                modelImporter.isReadable = true;
                modelImporter.SaveAndReimport();
            }
            else if (importer is TextureImporter texImporter)
            {
                texImporter.isReadable = true;
                texImporter.SaveAndReimport();
            }
            else if (path.EndsWith(".mat"))
            {
                Material mat = AssetDatabase.LoadAssetAtPath<Material>(path);
                if (mat != null)
                {
                    // 머터리얼 속성 변경 예시
                    mat.shader = Shader.Find("Standard");
                    EditorUtility.SetDirty(mat);
                }
            }
        }

        AssetDatabase.SaveAssets();
        Debug.Log("모든 모델, 텍스처, 머터리얼 에셋을 일괄 처리했습니다.");
    }
}
