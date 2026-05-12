#pragma once

class Mesh;
class BumpMesh;
class Model;

void AutoLOD_RegisterBumpMesh(BumpMesh* mesh);
Mesh* AutoLOD_GetLODMesh(Mesh* sourceMesh);

void AutoLOD_ResetPreloadQueue();
void AutoLOD_QueueMeshForPreload(Mesh* mesh);
void AutoLOD_QueueModelForPreload(Model* model);
void AutoLOD_ProcessPreloadQueue();
void AutoLOD_ProcessRuntimeQueue(int maxCount);

void AutoLOD_SetModelLODRenderActive(bool active);
bool AutoLOD_IsModelLODRenderActive();

void AutoLOD_BeginFrame();
void AutoLOD_RecordFrameSelection(bool usedLOD, bool resolvedLOD);
void AutoLOD_DebugUpdate(float deltaTime);
