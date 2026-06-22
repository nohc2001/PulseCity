#pragma once

#include <cstddef>
#include <vector>

class Mesh;
class BumpMesh;
class Model;

void AutoLOD_RegisterBumpMesh(BumpMesh* mesh);
void AutoLOD_OnBumpMeshReleased(BumpMesh* mesh);
const std::vector<BumpMesh*>& AutoLOD_GetGeneratedMeshes();
Mesh* AutoLOD_GetLODMesh(Mesh* sourceMesh);
Mesh* AutoLOD_GetLODMesh(Mesh* sourceMesh, int lodLevel);

void AutoLOD_ResetPreloadQueue();
void AutoLOD_QueueMeshForPreload(Mesh* mesh);
void AutoLOD_QueueModelForPreload(Model* model);
void AutoLOD_ProcessPreloadQueue();
void AutoLOD_ProcessRuntimeQueue(int maxCount);

void AutoLOD_SetModelLODRenderActive(bool active);
bool AutoLOD_IsModelLODRenderActive();
void AutoLOD_SetModelLODRenderLevel(int lodLevel);
int AutoLOD_GetModelLODRenderLevel();
int AutoLOD_GetSimpleMaterialIndex();

void AutoLOD_BeginFrame();
void AutoLOD_SetFrameStatsTracking(bool enabled);
bool AutoLOD_IsFrameStatsTracking();
void AutoLOD_RecordFrameSelection(bool usedLOD, bool resolvedLOD, size_t sourceTriangles = 0, size_t renderedTriangles = 0, size_t activeSourceTriangles = 0, size_t sourceDraws = 0, size_t renderedDraws = 0);
void AutoLOD_RecordFrameDraws(size_t sourceDraws, size_t renderedDraws);
void AutoLOD_RecordCombinedLOD1(bool used, size_t sourceDraws, size_t renderedDraws);
void AutoLOD_RecordBoxLOD(size_t boxCount, size_t meshProxyCount);
void AutoLOD_RecordFrameMiss(Mesh* sourceMesh, size_t sourceTriangles);
void AutoLOD_DebugUpdate(float deltaTime);
