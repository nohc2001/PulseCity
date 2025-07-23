/****************************************************************************************

   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.

   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.

****************************************************************************************/

#ifndef _DISPLAY_COMMON_H
#define _DISPLAY_COMMON_H

#include <fbxsdk.h>

void DisplayMetaDataConnections(FbxObject* pNode);
void DisplayString(const char* pHeader, const char* pValue  = "", const char* pSuffix  = "", bool linechange = true);
void DisplayBool(const char* pHeader, bool pValue, const char* pSuffix  = "", bool linechange = true);
void DisplayInt(const char* pHeader, int pValue, const char* pSuffix  = "", bool linechange = true);
void DisplayDouble(const char* pHeader, double pValue, const char* pSuffix  = "", bool linechange = true);
void Display2DVector(const char* pHeader, FbxVector2 pValue, const char* pSuffix  = "", bool linechange = true);
void Display3DVector(const char* pHeader, FbxVector4 pValue, const char* pSuffix  = "", bool linechange = true);
void DisplayColor(const char* pHeader, FbxColor pValue, const char* pSuffix  = "", bool linechange = true);
void Display4DVector(const char* pHeader, FbxVector4 pValue, const char* pSuffix  = "", bool linechange = true);


#endif // #ifndef _DISPLAY_COMMON_H


