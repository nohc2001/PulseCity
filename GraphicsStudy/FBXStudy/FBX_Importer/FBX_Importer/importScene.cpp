/****************************************************************************************
   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
****************************************************************************************/
//
// This example illustrates how to detect if a scene is password 
// protected, import and browse the scene to access node and animation 
// information. It displays the content of the FBX file which name is 
// passed as program argument. You can try it with the various FBX files 
// output by the export examples.
//
#include "Header.h"

#include "headers/Common.h"
#include "headers/DisplayCommon.h"
#include "headers/DisplayHierarchy.h"
#include "headers/DisplayAnimation.h"
#include "headers/DisplayMarker.h"
#include "headers/DisplaySkeleton.h"
#include "headers/DisplayMesh.h"
#include "headers/DisplayNurb.h"
#include "headers/DisplayPatch.h"
#include "headers/DisplayLodGroup.h"
#include "headers/DisplayCamera.h"
#include "headers/DisplayLight.h"
#include "headers/DisplayGlobalSettings.h"
#include "headers/DisplayPose.h"
#include "headers/DisplayPivotsAndLimits.h"
#include "headers/DisplayUserProperties.h"
#include "headers/DisplayGenericInfo.h"

// Local function prototypes.
void DisplayContent(FbxScene* pScene);
void DisplayContent(FbxNode* pNode);
void DisplayTarget(FbxNode* pNode);
void DisplayTransformPropagation(FbxNode* pNode);
void DisplayGeometricTransform(FbxNode* pNode);
void DisplayMetaData(FbxScene* pScene);

void UniformLCLScale(FbxNode* lNode, float mul);
static bool gVerbose = true;

bool view[128] = {};
std::vector<std::string> VisibleProperties;
// if size == 0, all property visible

int main()
{
    view[ViewContent::Marker] = false;
    view[ViewContent::Skeleton] = false;
    view[ViewContent::Mesh_Polygon] = false;
    view[ViewContent::Nurbs] = false;
    view[ViewContent::Patch] = false;
    view[ViewContent::Camera] = false;
    view[ViewContent::Light] = false;
    view[ViewContent::LODGroup] = false;
    view[ViewContent::Mesh_Texture] = true;
    view[ViewContent::Mesh_ControlPoint] = false;

    //VisibleProperties.emplace_back("PreRotation");

    FbxManager* lSdkManager = NULL;
    FbxScene* lScene = NULL;
    bool lResult;
    // Prepare the FBX SDK.
    InitializeSdkObjects(lSdkManager, lScene);
    // Load the scene.
    // The example can take a FBX file as an argument.

    //FbxString lFilePath("Mesh/default_door.fbx");
    FbxString lFilePath("Mesh/SMG_GUN/SMG GUN FREE.fbx");
    //FbxString lFilePath("Mesh/Lambroghini_Huracan_twin_Turbo_Lost/sto.fbx");

    FBXSDK_printf("\n\nFile: %s\n\n", lFilePath.Buffer());
    lResult = LoadScene(lSdkManager, lScene, lFilePath.Buffer());
    if (lResult == false)
    {
        FBXSDK_printf("\n\nAn error occurred while loading the scene...");
    }
    else
    {
        // Display the scene.
        DisplayMetaData(lScene);
        FBXSDK_printf("\n\n---------------------\nGlobal Light Settings\n---------------------\n\n");
        if (gVerbose) DisplayGlobalLightSettings(&lScene->GetGlobalSettings());

        FBXSDK_printf("\n\n----------------------\nGlobal Camera Settings\n----------------------\n\n");
        if (gVerbose) DisplayGlobalCameraSettings(&lScene->GetGlobalSettings());
        
        FBXSDK_printf("\n\n--------------------\nGlobal Time Settings\n--------------------\n\n");
        if (gVerbose) DisplayGlobalTimeSettings(&lScene->GetGlobalSettings());
        
        FBXSDK_printf("\n\n---------\nHierarchy\n---------\n\n");
        if (gVerbose) DisplayHierarchy(lScene);
        
        FBXSDK_printf("\n\n------------\nNode Content\n------------\n\n");
        if (gVerbose) DisplayContent(lScene);
        
        FBXSDK_printf("\n\n----\nPose\n----\n\n");
        if (gVerbose) DisplayPose(lScene);
        
        FBXSDK_printf("\n\n---------\nAnimation\n---------\n\n");
        if (gVerbose) DisplayAnimation(lScene);
        
        //now display generic information
        FBXSDK_printf("\n\n---------\nGeneric Information\n---------\n\n");
        if (gVerbose) DisplayGenericInfo(lScene);
    }

    UniformLCLScale(lScene->GetRootNode(), 0.1f);
    SaveScene(lSdkManager, lScene, "Mesh/SMG_GUN/SMG GUN FREE2.fbx");

    // Destroy all objects created by the FBX SDK.
    DestroySdkObjects(lSdkManager, lResult);
    return 0;
}
void DisplayContent(FbxScene* pScene)
{
    int i;
    FbxNode* lNode = pScene->GetRootNode();
    if (lNode)
    {
        for (i = 0; i < lNode->GetChildCount(); i++)
        {
            DisplayContent(lNode->GetChild(i));
        }
    }
}
void DisplayContent(FbxNode* pNode)
{
    FbxNodeAttribute::EType lAttributeType;
    int i;
    if (pNode->GetNodeAttribute() == NULL)
    {
        FBXSDK_printf("NULL Node Attribute\n\n");
    }
    else
    {
        lAttributeType = (pNode->GetNodeAttribute()->GetAttributeType());
        switch (lAttributeType)
        {
        default:
            break;
        case FbxNodeAttribute::eMarker:
            if(view[ViewContent::Marker]) DisplayMarker(pNode);
            break;
        case FbxNodeAttribute::eSkeleton:
            if (view[ViewContent::Skeleton]) DisplaySkeleton(pNode);
            break;
        case FbxNodeAttribute::eMesh:
            if (view[ViewContent::Mesh_Polygon] || view[ViewContent::Mesh_Texture]) DisplayMesh(pNode);
            break;
        case FbxNodeAttribute::eNurbs:
            if (view[ViewContent::Nurbs]) DisplayNurb(pNode);
            break;
        case FbxNodeAttribute::ePatch:
            if (view[ViewContent::Patch]) DisplayPatch(pNode);
            break;
        case FbxNodeAttribute::eCamera:
            if (view[ViewContent::Camera]) DisplayCamera(pNode);
            break;
        case FbxNodeAttribute::eLight:
            if (view[ViewContent::Light]) DisplayLight(pNode);
            break;
        case FbxNodeAttribute::eLODGroup:
            if (view[ViewContent::LODGroup]) DisplayLodGroup(pNode);
            break;
        }
    }
    DisplayUserProperties(pNode);
    DisplayTarget(pNode);
    DisplayPivotsAndLimits(pNode);
    DisplayTransformPropagation(pNode);
    DisplayGeometricTransform(pNode);
    for (i = 0; i < pNode->GetChildCount(); i++)
    {
        DisplayContent(pNode->GetChild(i));
    }
}
void DisplayTarget(FbxNode* pNode)
{
    if (pNode->GetTarget() != NULL)
    {
        DisplayString("    Target Name: ", (char*)pNode->GetTarget()->GetName());
    }
}
void DisplayTransformPropagation(FbxNode* pNode)
{
    FBXSDK_printf("    Transformation Propagation\n");
    // 
    // Rotation Space
    //
    EFbxRotationOrder lRotationOrder;
    pNode->GetRotationOrder(FbxNode::eSourcePivot, lRotationOrder);
    FBXSDK_printf("        Rotation Space: ");
    switch (lRotationOrder)
    {
    case eEulerXYZ:
        FBXSDK_printf("Euler XYZ\n");
        break;
    case eEulerXZY:
        FBXSDK_printf("Euler XZY\n");
        break;
    case eEulerYZX:
        FBXSDK_printf("Euler YZX\n");
        break;
    case eEulerYXZ:
        FBXSDK_printf("Euler YXZ\n");
        break;
    case eEulerZXY:
        FBXSDK_printf("Euler ZXY\n");
        break;
    case eEulerZYX:
        FBXSDK_printf("Euler ZYX\n");
        break;
    case eSphericXYZ:
        FBXSDK_printf("Spheric XYZ\n");
        break;
    }
    //
    // Use the Rotation space only for the limits
    // (keep using eEulerXYZ for the rest)
    //
    FBXSDK_printf("        Use the Rotation Space for Limit specification only: %s\n",
        pNode->GetUseRotationSpaceForLimitOnly(FbxNode::eSourcePivot) ? "Yes" : "No");
    //
    // Inherit Type
    //
    FbxTransform::EInheritType lInheritType;
    pNode->GetTransformationInheritType(lInheritType);
    FBXSDK_printf("        Transformation Inheritance: ");
    switch (lInheritType)
    {
    case FbxTransform::eInheritRrSs:
        FBXSDK_printf("RrSs\n");
        break;
    case FbxTransform::eInheritRSrs:
        FBXSDK_printf("RSrs\n");
        break;
    case FbxTransform::eInheritRrs:
        FBXSDK_printf("Rrs\n");
        break;
    }
}
void DisplayGeometricTransform(FbxNode* pNode)
{
    FbxVector4 lTmpVector;
    FBXSDK_printf("    Geometric Transformations\n");
    //
    // Translation
    //
    lTmpVector = pNode->GetGeometricTranslation(FbxNode::eSourcePivot);
    FBXSDK_printf("        Translation: %f %f %f\n", lTmpVector[0], lTmpVector[1], lTmpVector[2]);
    //
    // Rotation
    //
    lTmpVector = pNode->GetGeometricRotation(FbxNode::eSourcePivot);
    FBXSDK_printf("        Rotation:    %f %f %f\n", lTmpVector[0], lTmpVector[1], lTmpVector[2]);
    //
    // Scaling
    //
    lTmpVector = pNode->GetGeometricScaling(FbxNode::eSourcePivot);
    FBXSDK_printf("        Scaling:     %f %f %f\n", lTmpVector[0], lTmpVector[1], lTmpVector[2]);
}
void DisplayMetaData(FbxScene* pScene)
{
    FbxDocumentInfo* sceneInfo = pScene->GetSceneInfo();
    if (sceneInfo)
    {
        FBXSDK_printf("\n\n--------------------\nMeta-Data\n--------------------\n\n");
        FBXSDK_printf("    Title: %s\n", sceneInfo->mTitle.Buffer());
        FBXSDK_printf("    Subject: %s\n", sceneInfo->mSubject.Buffer());
        FBXSDK_printf("    Author: %s\n", sceneInfo->mAuthor.Buffer());
        FBXSDK_printf("    Keywords: %s\n", sceneInfo->mKeywords.Buffer());
        FBXSDK_printf("    Revision: %s\n", sceneInfo->mRevision.Buffer());
        FBXSDK_printf("    Comment: %s\n", sceneInfo->mComment.Buffer());
        FbxThumbnail* thumbnail = sceneInfo->GetSceneThumbnail();
        if (thumbnail)
        {
            FBXSDK_printf("    Thumbnail:\n");
            switch (thumbnail->GetDataFormat())
            {
            case FbxThumbnail::eRGB_24:
                FBXSDK_printf("        Format: RGB\n");
                break;
            case FbxThumbnail::eRGBA_32:
                FBXSDK_printf("        Format: RGBA\n");
                break;
            }
            switch (thumbnail->GetSize())
            {
            default:
                break;
            case FbxThumbnail::eNotSet:
                FBXSDK_printf("        Size: no dimensions specified (%ld bytes)\n", thumbnail->GetSizeInBytes());
                break;
            case FbxThumbnail::e64x64:
                FBXSDK_printf("        Size: 64 x 64 pixels (%ld bytes)\n", thumbnail->GetSizeInBytes());
                break;
            case FbxThumbnail::e128x128:
                FBXSDK_printf("        Size: 128 x 128 pixels (%ld bytes)\n", thumbnail->GetSizeInBytes());
            }
        }
    }
}

void UniformLCLScale(FbxNode* lNode, float mul) {
    int i;
    if (lNode)
    {
        //Get Self LCL Scale
        FbxObject* pObject = lNode;
        FbxProperty lProperty = pObject->GetFirstProperty();
        FbxDouble3 lcl_scale, one_scale;
        one_scale.mData[0] = 1;
        one_scale.mData[1] = 1;
        one_scale.mData[2] = 1;
        lcl_scale.mData[0] = 1;
        lcl_scale.mData[1] = 1;
        lcl_scale.mData[2] = 1;
        while (lProperty.IsValid())
        {
            FbxString lString;
            lString = lProperty.GetLabel();
            if (lString.Compare("Lcl Scaling") == 0) {
                lcl_scale = lProperty.Get<FbxDouble3>();
                lProperty.Set<FbxDouble3>(one_scale);
                break;
            }
            lProperty = pObject->GetNextProperty(lProperty);
        }

        FbxNodeAttribute::EType lAttributeType;
        int i;
        if (lNode->GetNodeAttribute() == NULL)
        {
            FBXSDK_printf("NULL Node Attribute\n\n");
        }
        else
        {
            lAttributeType = (lNode->GetNodeAttribute()->GetAttributeType());
            if (lAttributeType == FbxNodeAttribute::eMesh) {
                FbxMesh* pMesh = lNode->GetMesh();
                FbxVector4* lControlPoints = pMesh->GetControlPoints();
                int ctrlPos_count = pMesh->GetControlPointsCount();
                for (int k = 0; k < ctrlPos_count; ++k) {
                    lControlPoints[k].mData[0] *= lcl_scale.mData[0] * mul;
                    lControlPoints[k].mData[1] *= lcl_scale.mData[1] * mul;
                    lControlPoints[k].mData[2] *= lcl_scale.mData[2] * mul;
                }

                DisplayMesh(lNode);
            }
        }

        for (i = 0; i < lNode->GetChildCount(); i++)
        {
            UniformLCLScale(lNode->GetChild(i), mul);
        }
    }
}