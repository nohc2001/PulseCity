#include "pch.h"
#include <d3d12.h>
#include <D3DCompiler.h>
#include <dxcapi.h>
#include <Windows.h>
#include <stdio.h>

enum DEBUG_OUTPUT_TYPE
{
	DEBUG_OUTPUT_TYPE_NULL,
	DEBUG_OUTPUT_TYPE_CONSOLE,
	DEBUG_OUTPUT_TYPE_DEBUG_CONSOLE
};

inline void WriteDebugStringW(DEBUG_OUTPUT_TYPE type, const WCHAR* wchFormat, ...)
{
	va_list argptr;
	WCHAR cBuf[2048];

	va_start(argptr, wchFormat);
	vswprintf_s(cBuf, 2048, wchFormat, argptr);
	va_end(argptr);

	switch (type)
	{
	case DEBUG_OUTPUT_TYPE_NULL:
		break;
	case DEBUG_OUTPUT_TYPE_CONSOLE:
		wprintf_s(cBuf);
		break;
	case DEBUG_OUTPUT_TYPE_DEBUG_CONSOLE:
		OutputDebugStringW(cBuf);
		break;
	}
}

inline void WriteDebugStringA(DEBUG_OUTPUT_TYPE type, const char* szFormat, ...)
{
	va_list argptr;
	char cBuf[2048];

	va_start(argptr, szFormat);
	vsprintf_s(cBuf, 2048, szFormat, argptr);
	va_end(argptr);

	switch (type)
	{
	case DEBUG_OUTPUT_TYPE_NULL:
		break;
	case DEBUG_OUTPUT_TYPE_CONSOLE:
		printf_s(cBuf);
		break;
	case DEBUG_OUTPUT_TYPE_DEBUG_CONSOLE:
		OutputDebugStringA(cBuf);
		break;
	}
}


#include "ShaderUtil.h"

BOOL CreateShaderCodeFromFile(BYTE** ppOutCodeBuffer, DWORD* pdwOutCodeSize, SYSTEMTIME* pOutLastWriteTime, const WCHAR* wchFileName)
{
	BOOL	bResult = FALSE;

	DWORD	dwOpenFlags = OPEN_EXISTING;
	DWORD	dwAccessMode = GENERIC_READ;
	DWORD	dwShare = FILE_SHARE_READ;

	WCHAR	wchTxt[256] = {};

	//HANDLE	hFile = CreateFileA(szFileName,dwAccessMode, dwShare, nullptr,dwOpenFlags,FILE_ATTRIBUTE_NORMAL,nullptr);

	CREATEFILE2_EXTENDED_PARAMETERS extendedParams = {};
	extendedParams.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
	extendedParams.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
	extendedParams.dwFileFlags = FILE_FLAG_SEQUENTIAL_SCAN;
	extendedParams.dwSecurityQosFlags = SECURITY_ANONYMOUS;
	extendedParams.lpSecurityAttributes = nullptr;
	extendedParams.hTemplateFile = nullptr;

	HANDLE	hFile = CreateFile2(wchFileName, dwAccessMode, dwShare, dwOpenFlags, &extendedParams);

	DWORD dwFileSize;
	DWORD dwCodeSize;
	BYTE* pCodeBuffer;
	DWORD dwReadBytes = 0;
	FILETIME createTime, lastAccessTime, lastWriteTime;
	SYSTEMTIME	sysLastWriteTime;

	if (INVALID_HANDLE_VALUE == hFile)
	{
		swprintf_s(wchTxt, L"Shader File Not Found : %s\n", wchFileName);
		OutputDebugStringW(wchTxt);
		goto lb_return;
	}

	dwFileSize = GetFileSize(hFile, nullptr);
	if (dwFileSize > 1024 * 1024)
	{
		swprintf_s(wchTxt, L"Invalid Shader File : %s\n", wchFileName);
		OutputDebugStringW(wchTxt);
		goto lb_close_return;
	}
	dwCodeSize = dwFileSize + 1;

	pCodeBuffer = new BYTE[dwCodeSize];
	memset(pCodeBuffer, 0, dwCodeSize);

	if (!ReadFile(hFile, pCodeBuffer, dwFileSize, &dwReadBytes, nullptr))
	{
		swprintf_s(wchTxt, L"Failed to Read File : %s\n", wchFileName);
		OutputDebugStringW(wchTxt);
		goto lb_close_return;
	}
	
	GetFileTime(hFile, &createTime, &lastAccessTime, &lastWriteTime);

	FileTimeToSystemTime(&lastWriteTime, &sysLastWriteTime);

	*ppOutCodeBuffer = pCodeBuffer;
	*pdwOutCodeSize = dwCodeSize;
	*pOutLastWriteTime = sysLastWriteTime;
	bResult = TRUE;

lb_close_return:
	CloseHandle(hFile);

lb_return:
	return bResult;

}

void DeleteShaderCode(BYTE* pCodeBuffer)
{
	delete[] pCodeBuffer;
}

HRESULT CompileShaderFromFileWithDXC(IDxcLibrary* pLibrary, IDxcCompiler* pCompiler, IDxcIncludeHandler* pIncludeHandler, const WCHAR* wchFileName, const WCHAR* wchEntryPoint, const WCHAR* wchShaderModel, IDxcBlob** ppOutCodeBlob, BOOL bDisableOptimize, SYSTEMTIME* pOutLastWriteTime, DWORD dwFlags)
{
	HRESULT hr = E_FAIL;

	SYSTEMTIME	lastWriteTime;
	BYTE*		pCodeBuffer = nullptr;
	DWORD		dwCodeSize = 0;

	if (!CreateShaderCodeFromFile(&pCodeBuffer, &dwCodeSize, &lastWriteTime, wchFileName))
	{
		return E_FAIL;
	}

	DWORD	dwOptimizeFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
	if (bDisableOptimize)
	{
		// Enable better shader debugging with the graphics debugging tools.
		dwOptimizeFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	}

	DWORD	dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
	dwShaderFlags |= dwOptimizeFlags;

	/*
	{
		L"-Zpr",			//Row-major matrices
		L"-WX",				//Warnings as errors
#ifdef _DEBUG
		L"-Zi",				//Debug info
		L"-Qembed_debug",	//Embed debug info into the shader
		L"-Od",				//Disable optimization
#else
		L"-O3",				//Optimization level 3
#endif
	};
	*/

	LPCWSTR pArg[16] = {};
	DWORD dwArgCount = 0;
	if (bDisableOptimize)
	{
		pArg[dwArgCount] = L"-Zi";
		dwArgCount++;
		pArg[dwArgCount] = L"-Qembed_debug";
		dwArgCount++;
		pArg[dwArgCount] = L"-Od";
		dwArgCount++;
	}
	else
	{
		pArg[dwArgCount] = L"-O3";				//Optimization level 3
		dwArgCount++;
	}


	IDxcBlobEncoding*	pCodeTextBlob = nullptr;
	hr = pLibrary->CreateBlobWithEncodingFromPinned(pCodeBuffer, (UINT32)dwCodeSize, CP_ACP, &pCodeTextBlob);
	if (FAILED(hr))
		__debugbreak();

	IDxcOperationResult* pCompileResult = nullptr;
	hr = pCompiler->Compile(pCodeTextBlob, wchFileName, wchEntryPoint, wchShaderModel, pArg, dwArgCount, nullptr, 0, pIncludeHandler, &pCompileResult);


	HRESULT hrCompile;
	hr = pCompileResult->GetStatus(&hrCompile);

	if (SUCCEEDED(hrCompile))
	{
		pCompileResult->GetResult(ppOutCodeBlob);
	}
	else
	{
		IDxcBlobEncoding* pErrorBlob = nullptr;
		hr = pCompileResult->GetErrorBuffer(&pErrorBlob);
		const char* szErrMsg = (const char*)pErrorBlob->GetBufferPointer();
		WriteDebugStringA(DEBUG_OUTPUT_TYPE_DEBUG_CONSOLE, "Failed Compile Shader: %s\n", szErrMsg);
		if (pErrorBlob)
		{
			pErrorBlob->Release();
			pErrorBlob = nullptr;
		}
	}

	if (pCompileResult)
	{
		pCompileResult->Release();
		pCompileResult = nullptr;
	}
	if (pCodeTextBlob)
	{
		pCodeTextBlob->Release();
		pCodeTextBlob = nullptr;
	}

	//	ID3DBlob* pErrorBlob = nullptr;
	//	hr = D3DCompile2(pCodeBuffer, (size_t)dwCodeSize, szFileName, pDefine, D3D_COMPILE_STANDARD_FILE_INCLUDE, szEntryPoint, szShaderModel, dwShaderFlags, 0, 0, nullptr, 0, ppBlobOut, &pErrorBlob);

	DeleteShaderCode(pCodeBuffer);
	*pOutLastWriteTime = lastWriteTime;

	return hrCompile;
}