/****************************************************************************************

   Copyright (C) 2020 Autodesk, Inc.
   All rights reserved.

   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

#include <fbxsdk.h>

#include "DisplayCommon.h"

static const FbxImplementation* LookForImplementation(FbxSurfaceMaterial* pMaterial)
{
    const FbxImplementation* lImplementation = nullptr;
    if (!lImplementation) lImplementation = GetImplementation(pMaterial, FBXSDK_IMPLEMENTATION_CGFX);
    if (!lImplementation) lImplementation = GetImplementation(pMaterial, FBXSDK_IMPLEMENTATION_HLSL);
    if (!lImplementation) lImplementation = GetImplementation(pMaterial, FBXSDK_IMPLEMENTATION_SFX);
    if (!lImplementation) lImplementation = GetImplementation(pMaterial, FBXSDK_IMPLEMENTATION_OGS);
    if (!lImplementation) lImplementation = GetImplementation(pMaterial, FBXSDK_IMPLEMENTATION_SSSL);
    return lImplementation;    
}

void DisplayMaterial(FbxGeometry* pGeometry)
{
    int lMaterialCount = 0;
    FbxNode* lNode = NULL;
    if(pGeometry){
        lNode = pGeometry->GetNode();
        if(lNode)
            lMaterialCount = lNode->GetMaterialCount();    
    }

    if (lMaterialCount > 0)
    {
        FbxPropertyT<FbxDouble3> lKFbxDouble3;
        FbxPropertyT<FbxDouble> lKFbxDouble1;
        FbxColor theColor;

        for (int lCount = 0; lCount < lMaterialCount; lCount ++)
        {
            DisplayInt("        Material ", lCount);

            FbxSurfaceMaterial *lMaterial = lNode->GetMaterial(lCount);

            DisplayString("            Name: \"", (char *) lMaterial->GetName(), "\""); 

            //Get the implementation to see if it's a hardware shader.
            const FbxImplementation* lImplementation = LookForImplementation(lMaterial);
            if(lImplementation)
            {
                //Now we have a hardware shader, let's read it
                DisplayString("            Language: ", lImplementation->Language.Get().Buffer());
                DisplayString("            LanguageVersion: ", lImplementation->LanguageVersion.Get().Buffer());
                DisplayString("            RenderName: ", lImplementation->RenderName.Buffer());
                DisplayString("            RenderAPI: ", lImplementation->RenderAPI.Get().Buffer());
                DisplayString("            RenderAPIVersion: ", lImplementation->RenderAPIVersion.Get().Buffer());

                const FbxBindingTable* lRootTable = lImplementation->GetRootTable();
                FbxString lFileName = lRootTable->DescAbsoluteURL.Get();
                FbxString lTechniqueName = lRootTable->DescTAG.Get(); 


                const FbxBindingTable* lTable = lImplementation->GetRootTable();
                size_t lEntryNum = lTable->GetEntryCount();

                for(int i=0;i <(int)lEntryNum; ++i)
                {
                    const FbxBindingTableEntry& lEntry = lTable->GetEntry(i);
                    const char* lEntrySrcType = lEntry.GetEntryType(true); 
                    FbxProperty lFbxProp;

                    FbxString lTest = lEntry.GetSource();
                    DisplayString("            Entry: ", lTest.Buffer());


                    if ( strcmp( FbxPropertyEntryView::sEntryType, lEntrySrcType ) == 0 )
                    {   
                        lFbxProp = lMaterial->FindPropertyHierarchical(lEntry.GetSource()); 
                        if(!lFbxProp.IsValid())
                        {
                            lFbxProp = lMaterial->RootProperty.FindHierarchical(lEntry.GetSource());
                        }


                    }
                    else if( strcmp( FbxConstantEntryView::sEntryType, lEntrySrcType ) == 0 )
                    {
                        lFbxProp = lImplementation->GetConstants().FindHierarchical(lEntry.GetSource());
                    }
                    if(lFbxProp.IsValid())
                    {
                        if( lFbxProp.GetSrcObjectCount<FbxTexture>() > 0 )
                        {
                            //do what you want with the textures
                            for(int j=0; j<lFbxProp.GetSrcObjectCount<FbxFileTexture>(); ++j)
                            {
                                FbxFileTexture *lTex = lFbxProp.GetSrcObject<FbxFileTexture>(j);
                                DisplayString("           File Texture: ", lTex->GetFileName());
                            }
                            for(int j=0; j<lFbxProp.GetSrcObjectCount<FbxLayeredTexture>(); ++j)
                            {
                                FbxLayeredTexture *lTex = lFbxProp.GetSrcObject<FbxLayeredTexture>(j);
                                DisplayString("        Layered Texture: ", lTex->GetName());
                            }
                            for(int j=0; j<lFbxProp.GetSrcObjectCount<FbxProceduralTexture>(); ++j)
                            {
                                FbxProceduralTexture *lTex = lFbxProp.GetSrcObject<FbxProceduralTexture>(j);
                                DisplayString("     Procedural Texture: ", lTex->GetName());
                            }
                        }
                        else
                        {
                            FbxDataType lFbxType = lFbxProp.GetPropertyDataType();
                            FbxString blah = lFbxType.GetName();
                            if(FbxBoolDT == lFbxType)
                            {
                                DisplayBool("                Bool: ", lFbxProp.Get<FbxBool>() );
                            }
                            else if ( FbxIntDT == lFbxType ||  FbxEnumDT  == lFbxType )
                            {
                                DisplayInt("                Int: ", lFbxProp.Get<FbxInt>());
                            }
                            else if ( FbxFloatDT == lFbxType)
                            {
                                DisplayDouble("                Float: ", lFbxProp.Get<FbxFloat>());

                            }
                            else if ( FbxDoubleDT == lFbxType)
                            {
                                DisplayDouble("                Double: ", lFbxProp.Get<FbxDouble>());
                            }
                            else if ( FbxStringDT == lFbxType
                                ||  FbxUrlDT  == lFbxType
                                ||  FbxXRefUrlDT  == lFbxType )
                            {
                                DisplayString("                String: ", lFbxProp.Get<FbxString>().Buffer());
                            }
                            else if ( FbxDouble2DT == lFbxType)
                            {
                                FbxDouble2 lDouble2 = lFbxProp.Get<FbxDouble2>();
                                FbxVector2 lVect;
                                lVect[0] = lDouble2[0];
                                lVect[1] = lDouble2[1];

                                Display2DVector("                2D vector: ", lVect);
                            }
                            else if ( FbxDouble3DT == lFbxType || FbxColor3DT == lFbxType)
                            {
                                FbxDouble3 lDouble3 = lFbxProp.Get<FbxDouble3>();


                                FbxVector4 lVect;
                                lVect[0] = lDouble3[0];
                                lVect[1] = lDouble3[1];
                                lVect[2] = lDouble3[2];
                                Display3DVector("                3D vector: ", lVect);
                            }

                            else if ( FbxDouble4DT == lFbxType || FbxColor4DT == lFbxType)
                            {
                                FbxDouble4 lDouble4 = lFbxProp.Get<FbxDouble4>();
                                FbxVector4 lVect;
                                lVect[0] = lDouble4[0];
                                lVect[1] = lDouble4[1];
                                lVect[2] = lDouble4[2];
                                lVect[3] = lDouble4[3];
                                Display4DVector("                4D vector: ", lVect);
                            }
                            else if ( FbxDouble4x4DT == lFbxType)
                            {
                                FbxDouble4x4 lDouble44 = lFbxProp.Get<FbxDouble4x4>();
                                for(int j=0; j<4; ++j)
                                {

                                    FbxVector4 lVect;
                                    lVect[0] = lDouble44[j][0];
                                    lVect[1] = lDouble44[j][1];
                                    lVect[2] = lDouble44[j][2];
                                    lVect[3] = lDouble44[j][3];
                                    Display4DVector("                4x4D vector: ", lVect);
                                }

                            }
                        }

                    }   
                }
            }
            else if (lMaterial->GetClassId().Is(FbxSurfacePhong::ClassId))
            {
                // We found a Phong material.  Display its properties.

                // Display the Ambient Color
                lKFbxDouble3 =((FbxSurfacePhong *) lMaterial)->Ambient;
                theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
                DisplayColor("            Ambient: ", theColor);

                // Display the Diffuse Color
                lKFbxDouble3 =((FbxSurfacePhong *) lMaterial)->Diffuse;
                theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
                DisplayColor("            Diffuse: ", theColor);

                // Display the Specular Color (unique to Phong materials)
                lKFbxDouble3 =((FbxSurfacePhong *) lMaterial)->Specular;
                theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
                DisplayColor("            Specular: ", theColor);

                // Display the Emissive Color
                lKFbxDouble3 =((FbxSurfacePhong *) lMaterial)->Emissive;
                theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
                DisplayColor("            Emissive: ", theColor);

                //Opacity is Transparency factor now
                lKFbxDouble1 =((FbxSurfacePhong *) lMaterial)->TransparencyFactor;
                DisplayDouble("            Opacity: ", 1.0-lKFbxDouble1.Get());

                // Display the Shininess
                lKFbxDouble1 =((FbxSurfacePhong *) lMaterial)->Shininess;
                DisplayDouble("            Shininess: ", lKFbxDouble1.Get());

                // Display the Reflectivity
                lKFbxDouble1 =((FbxSurfacePhong *) lMaterial)->ReflectionFactor;
                DisplayDouble("            Reflectivity: ", lKFbxDouble1.Get());
            }
            else if(lMaterial->GetClassId().Is(FbxSurfaceLambert::ClassId) )
            {
                // We found a Lambert material. Display its properties.
                // Display the Ambient Color
                lKFbxDouble3=((FbxSurfaceLambert *)lMaterial)->Ambient;
                theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
                DisplayColor("            Ambient: ", theColor);

                // Display the Diffuse Color
                lKFbxDouble3 =((FbxSurfaceLambert *)lMaterial)->Diffuse;
                theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
                DisplayColor("            Diffuse: ", theColor);

                // Display the Emissive
                lKFbxDouble3 =((FbxSurfaceLambert *)lMaterial)->Emissive;
                theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
                DisplayColor("            Emissive: ", theColor);

                // Display the Opacity
                lKFbxDouble1 =((FbxSurfaceLambert *)lMaterial)->TransparencyFactor;
                DisplayDouble("            Opacity: ", 1.0-lKFbxDouble1.Get());
            }
            else
                DisplayString("Unknown type of Material");

            FbxPropertyT<FbxString> lString;
            lString = lMaterial->ShadingModel;

            DisplayString("            Shading Model: ", lString.Get().Buffer());
            DisplayString("");
            
            //이건 텍스쳐 스트링이 아닌듯..?
            /*printf("MultiLayer : %s\n", lMaterial->sMultiLayer);
            printf("Emissive : %s\n", lMaterial->sEmissive);
            printf("Emissive Factor : %s\n", lMaterial->sEmissiveFactor);
            printf("Ambient : %s\n", lMaterial->sAmbient);
            printf("Ambient Factor : %s\n", lMaterial->sAmbientFactor);
            printf("Diffuse : %s\n", lMaterial->sDiffuse);
            printf("Diffuse Factor : %s\n", lMaterial->sDiffuseFactor);
            printf("Specular : %s\n", lMaterial->sSpecular);
            printf("Specular Factor : %s\n", lMaterial->sSpecularFactor);
            printf("Shininess : %s\n", lMaterial->sShininess);
            printf("Bump : %s\n", lMaterial->sBump);
            printf("NormalMap : %s\n", lMaterial->sNormalMap);
            printf("BumpFactor : %s\n", lMaterial->sBumpFactor);
            printf("TransparentColor : %s\n", lMaterial->sTransparentColor);
            printf("TransparencyFactor : %s\n", lMaterial->sTransparencyFactor);
            printf("Reflection : %s\n", lMaterial->sReflection);
            printf("Reflection Factor : %s\n", lMaterial->sReflectionFactor);
            printf("Displacement Color : %s\n", lMaterial->sDisplacementColor);
            printf("Displacement Factor : %s\n", lMaterial->sDisplacementFactor);
            printf("Vector Displacement Color : %s\n", lMaterial->sVectorDisplacementColor);
            printf("Vector Displacement Factor : %s\n", lMaterial->sVectorDisplacementFactor);*/
        }
    }
}

