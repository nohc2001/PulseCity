/****************************************************************************************

   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.

   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.

****************************************************************************************/

#include "../Header.h"
#include <fbxsdk.h>

#include "DisplayCommon.h"

void DisplayProperties(FbxObject* pObject);
void DisplayGenericInfo(FbxNode* pNode, int pDepth);
void DisplayGenericInfo(FbxScene* pScene)
{
    int i;
    FbxNode* lRootNode = pScene->GetRootNode();

    for( i = 0; i < lRootNode->GetChildCount(); i++)
    {
        DisplayGenericInfo(lRootNode->GetChild(i), 0);
    }

	//Other objects directly connected onto the scene
	for( i = 0; i < pScene->GetSrcObjectCount(); ++i )
	{
		DisplayProperties(pScene->GetSrcObject(i));
	}
}


void DisplayGenericInfo(FbxNode* pNode, int pDepth)
{
    FbxString lString;
    int i;

    for(i = 0; i < pDepth; i++)
    {
        lString += "     ";
    }

    lString += pNode->GetName();
    lString += "\n";

    DisplayString(lString.Buffer());

    //Display generic info about that Node
    DisplayProperties(pNode);
    DisplayString("");
    for(i = 0; i < pNode->GetChildCount(); i++)
    {
        DisplayGenericInfo(pNode->GetChild(i), pDepth + 1);
    }
}

void DisplayProperties(FbxObject* pObject)
{

    DisplayString("Name: ", (char *)pObject->GetName());

    // Display all the properties
    int i,  lCount = 0;
    FbxProperty lProperty = pObject->GetFirstProperty();
    while (lProperty.IsValid())
    {
        lCount++;
        lProperty = pObject->GetNextProperty(lProperty);
    }

    FbxString lTitleStr = "    Property Count: ";

    if (lCount == 0)
        return; // there are no properties to display

    DisplayInt(lTitleStr.Buffer(), lCount);

    i=0;
	lProperty = pObject->GetFirstProperty();
    while (lProperty.IsValid())
    {
        // exclude user properties

        FbxString lString;
        lString = lProperty.GetLabel();

        int cmpmul = 1;
        if (VisibleProperties.size() == 0) cmpmul = 0;
        for (int k = 0; k < VisibleProperties.size(); ++k) {
            cmpmul *= lString.Compare(VisibleProperties[k].c_str());
        }
        if (cmpmul != 0) {
            i++;
            lProperty = pObject->GetNextProperty(lProperty);
            continue;
        }

        //DisplayInt("        Property ", i);
        //lString = lProperty.GetName();
        //DisplayString("            Internal Name: ", lString.Buffer());
        printf("        Property : %s %s = {", lProperty.GetPropertyDataType().GetName(), lProperty.GetLabel().Buffer());
        switch (lProperty.GetPropertyDataType().GetType())
        {
        case eFbxBool:
            DisplayBool("", lProperty.Get<FbxBool>(), "", false);
            break;

        case eFbxDouble:
            DisplayDouble("", lProperty.Get<FbxDouble>(), "", false);
            break;

        case eFbxDouble4:
            {
                FbxColor lDefault;
                char      lBuf[64];

                lDefault = lProperty.Get<FbxColor>();
                FBXSDK_sprintf(lBuf, 64, "R=%f, G=%f, B=%f, A=%f", lDefault.mRed, lDefault.mGreen, lDefault.mBlue, lDefault.mAlpha);
                DisplayString("", lBuf, "", false);
            }
            break;

        case eFbxInt:
            DisplayInt("", lProperty.Get<FbxInt>(), "", false);
            break;

        case eFbxDouble3:
            {
                FbxDouble3 lDefault;
                char   lBuf[64];

                lDefault = lProperty.Get<FbxDouble3>();
                FBXSDK_sprintf(lBuf, 64, "X=%g, Y=%g, Z=%g", lDefault[0], lDefault[1], lDefault[2]);
                DisplayString("", lBuf, "", false);
            }
            break;
        case eFbxEnum:
            DisplayInt("", lProperty.Get<FbxEnum>(), "", false);
            break;

        case eFbxFloat:
            DisplayDouble("", lProperty.Get<FbxFloat>(), "", false);
            break;
        case eFbxString:
            lString = lProperty.Get<FbxString>();
            DisplayString("", lString.Buffer(), "", false);
            break;

        default:
            DisplayString("UNIDENTIFIED", "", "", false);
            break;
        }

        printf("}");
        if (lProperty.HasMinLimit()) DisplayDouble(" MinMax:{", lProperty.GetMinLimit(), "", false);
        if (lProperty.HasMaxLimit()) DisplayDouble("", lProperty.GetMaxLimit(), "}", false);
        if (lProperty.GetFlag(FbxPropertyFlags::eAnimatable)) DisplayBool(" Is Animatable: ", lProperty.GetFlag(FbxPropertyFlags::eAnimatable), "", false);

        printf("\n");

        i++;
        lProperty = pObject->GetNextProperty(lProperty);
    }
}

