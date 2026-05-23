#pragma once

#include <cstddef>

class Mesh;
class BumpMesh;
class Model;

void AutoLOD_RegisterBumpMesh(BumpMesh* mesh);
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

void AutoLOD_BeginFrame();
void AutoLOD_RecordFrameSelection(bool usedLOD, bool resolvedLOD, size_t sourceTriangles = 0, size_t renderedTriangles = 0, size_t activeSourceTriangles = 0);
void AutoLOD_RecordFrameMiss(Mesh* sourceMesh, size_t sourceTriangles);
void AutoLOD_DebugUpdate(float deltaTime);
