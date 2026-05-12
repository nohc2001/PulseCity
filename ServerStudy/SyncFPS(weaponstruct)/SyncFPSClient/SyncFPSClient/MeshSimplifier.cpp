#include "stdafx.h"
#include "Render.h"
#include "MeshSimplifier.h"

#include "third_party/meshoptimizer/src/meshoptimizer.h"

#include <algorithm>
#include <cfloat>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <unordered_map>

namespace {
	constexpr unsigned int kAutoLODCacheMagic = 0x31444F4C; // LOD1
	constexpr unsigned int kAutoLODCacheVersion = 25;
	constexpr float kAutoLODRatio = 0.35f;
	constexpr float kAutoLODTargetError = 5e-4f;
	constexpr float kAutoLODDebugInterval = 1.0f;
	constexpr float kAutoLODPreloadLargeVolume = 8.0f;
	constexpr int kAutoLODRuntimeBuildsPerTick = 2;
	constexpr int kAutoLODMinSubsetTriangles = 54;
	constexpr int kAutoLODMinResultTriangles = 84;
	constexpr float kAutoLODMinAcceptedSubsetRatio = 0.30f;
	constexpr float kAutoLODNormalProtectDot = 0.10f;
	constexpr float kAutoLODTangentProtectDot = -0.10f;
	constexpr float kAutoLODNormalPriorityDot = 0.60f;
	constexpr float kAutoLODTangentPriorityDot = 0.40f;
	constexpr float kAutoLODSubMeshPriorityNormalDot = 0.60f;
	constexpr float kAutoLODSubMeshPriorityTangentDot = 0.40f;
	constexpr float kAutoLODUVProtectEpsilon = 1e-4f;
	constexpr float kAutoLODPositionRemapTolerance = 1e-3f;
	constexpr float kAutoLODMaxAspectRatio = 18.0f;
	constexpr float kAutoLODMaxProtectedVertexRatio = 0.995f;
	constexpr size_t kAutoLODMaxSubMeshCount = 45;

	enum class RemapPairPolicy {
		None,
		Priority,
		Protect,
	};

	float Dot3(const XMFLOAT3& a, const XMFLOAT3& b);
	void Normalize3(XMFLOAT3& v);
	RemapPairPolicy GetRemapPairPolicy(const BumpMesh::Vertex& a, unsigned int aSubMeshId, const BumpMesh::Vertex& b, unsigned int bSubMeshId);
	const char* GetAutoLODSkipReason();

	struct AutoLODCacheHeader {
		unsigned int magic = kAutoLODCacheMagic;
		unsigned int version = kAutoLODCacheVersion;
		unsigned int vertexCount = 0;
		unsigned int triangleCount = 0;
		unsigned int subMeshCount = 0;
		float ratio = kAutoLODRatio;
		XMFLOAT3 obbExt = {};
		XMFLOAT3 obbTr = {};
	};

	struct AutoLODMeshData {
		std::vector<BumpMesh::Vertex> vertices;
		std::vector<TriangleIndex> indices;
		std::vector<int> subMeshIndexStart;
	};

	struct AutoLODAttr {
		float nx, ny, nz;
		float u, v;
		float tx, ty, tz;
	};

	struct PreloadEntry {
		BumpMesh* mesh = nullptr;
		float priority = 0.0f;
	};

	std::unordered_map<const Mesh*, BumpMesh*> gAutoLODMeshes;
	std::unordered_map<const Mesh*, BumpMesh*> gRegisteredSourceMeshes;
	std::vector<BumpMesh*> gAutoLODOwnedMeshes;
	std::vector<PreloadEntry> gPreloadQueue;
	std::unordered_set<const Mesh*> gQueuedPreloadMeshes;
	std::vector<PreloadEntry> gRuntimeQueue;
	bool gAutoLODModelRenderActive = false;
	thread_local const char* gAutoLODSkipReason = nullptr;

	bool gAutoLODTrackFrameStats = false;
	int gAutoLODFrameTotal = 0;
	int gAutoLODFrameActive = 0;
	int gAutoLODFrameResolved = 0;
	float gAutoLODDebugTimer = 0.0f;

	int gAutoLODCacheHits = 0;
	int gAutoLODCacheMisses = 0;
	int gAutoLODBuilds = 0;
	int gAutoLODSkips = 0;
	int gAutoLODPreloadProcessed = 0;
	int gAutoLODPreloadTotal = 0;
	int gAutoLODRuntimeProcessed = 0;
	int gAutoLODRuntimeTotal = 0;

	uint64_t HashAppend(uint64_t hash, const void* data, size_t size)
	{
		const unsigned char* ptr = reinterpret_cast<const unsigned char*>(data);
		for (size_t i = 0; i < size; ++i) {
			hash ^= uint64_t(ptr[i]);
			hash *= 1099511628211ull;
		}
		return hash;
	}

	uint64_t BuildMeshHash(const BumpMesh* mesh)
	{
		uint64_t hash = 1469598103934665603ull;
		hash = HashAppend(hash, &kAutoLODCacheVersion, sizeof(kAutoLODCacheVersion));
		hash = HashAppend(hash, &kAutoLODRatio, sizeof(kAutoLODRatio));
		hash = HashAppend(hash, mesh->sourceVertexData.data(), mesh->sourceVertexData.size() * sizeof(BumpMesh::Vertex));
		hash = HashAppend(hash, mesh->sourceIndexData.data(), mesh->sourceIndexData.size() * sizeof(TriangleIndex));
		hash = HashAppend(hash, mesh->sourceSubMeshIndexStart.data(), mesh->sourceSubMeshIndexStart.size() * sizeof(int));
		return hash;
	}

	std::filesystem::path GetCacheDir()
	{
		return std::filesystem::path("Cache") / "AutoLOD";
	}

	std::filesystem::path GetCachePath(const BumpMesh* mesh)
	{
		char buf[64];
		sprintf_s(buf, "mesh_%016llx.bin", static_cast<unsigned long long>(BuildMeshHash(mesh)));
		return GetCacheDir() / buf;
	}

	bool SaveCacheFile(const std::filesystem::path& path, const BumpMesh* sourceMesh, const AutoLODMeshData& data)
	{
		std::filesystem::create_directories(path.parent_path());

		AutoLODCacheHeader header;
		header.vertexCount = static_cast<unsigned int>(data.vertices.size());
		header.triangleCount = static_cast<unsigned int>(data.indices.size());
		header.subMeshCount = static_cast<unsigned int>(data.subMeshIndexStart.size() > 0 ? data.subMeshIndexStart.size() - 1 : 0);
		header.obbExt = sourceMesh->OBB_Ext;
		header.obbTr = sourceMesh->OBB_Tr;

		std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
		if (!ofs.is_open()) return false;

		ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));
		ofs.write(reinterpret_cast<const char*>(data.subMeshIndexStart.data()), data.subMeshIndexStart.size() * sizeof(int));
		ofs.write(reinterpret_cast<const char*>(data.vertices.data()), data.vertices.size() * sizeof(BumpMesh::Vertex));
		ofs.write(reinterpret_cast<const char*>(data.indices.data()), data.indices.size() * sizeof(TriangleIndex));
		return ofs.good();
	}

	bool LoadCacheFile(const std::filesystem::path& path, AutoLODCacheHeader& header, AutoLODMeshData& data)
	{
		std::ifstream ifs(path, std::ios::binary);
		if (!ifs.is_open()) return false;

		ifs.read(reinterpret_cast<char*>(&header), sizeof(header));
		if (!ifs.good()) return false;
		if (header.magic != kAutoLODCacheMagic || header.version != kAutoLODCacheVersion) return false;

		data.subMeshIndexStart.resize(header.subMeshCount + 1);
		data.vertices.resize(header.vertexCount);
		data.indices.resize(header.triangleCount);

		ifs.read(reinterpret_cast<char*>(data.subMeshIndexStart.data()), data.subMeshIndexStart.size() * sizeof(int));
		ifs.read(reinterpret_cast<char*>(data.vertices.data()), data.vertices.size() * sizeof(BumpMesh::Vertex));
		ifs.read(reinterpret_cast<char*>(data.indices.data()), data.indices.size() * sizeof(TriangleIndex));
		return ifs.good();
	}

	bool BuildSimplifiedMeshData(const BumpMesh* sourceMesh, AutoLODMeshData& outData)
	{
		gAutoLODSkipReason = nullptr;
		if (sourceMesh->sourceVertexData.empty() || sourceMesh->sourceIndexData.empty() || sourceMesh->sourceSubMeshIndexStart.size() < 2) {
			gAutoLODSkipReason = "invalid-source";
			return false;
		}

		const size_t subMeshCount = sourceMesh->sourceSubMeshIndexStart.size() - 1;
		if (subMeshCount > kAutoLODMaxSubMeshCount) {
			gAutoLODSkipReason = "too-many-submeshes";
			return false;
		}

		const float ex = (sourceMesh->OBB_Ext.x > 1e-4f) ? sourceMesh->OBB_Ext.x : 1e-4f;
		const float ey = (sourceMesh->OBB_Ext.y > 1e-4f) ? sourceMesh->OBB_Ext.y : 1e-4f;
		const float ez = (sourceMesh->OBB_Ext.z > 1e-4f) ? sourceMesh->OBB_Ext.z : 1e-4f;
		const float maxExt = (ex > ey) ? ((ex > ez) ? ex : ez) : ((ey > ez) ? ey : ez);
		const float minExt = (ex < ey) ? ((ex < ez) ? ex : ez) : ((ey < ez) ? ey : ez);
		if ((maxExt / minExt) >= kAutoLODMaxAspectRatio) {
			gAutoLODSkipReason = "thin-structure";
			return false;
		}

		const float horizontalMax = (ex > ez) ? ex : ez;
		const float horizontalMin = (ex < ez) ? ex : ez;
		const bool lowProfileWideMesh =
			horizontalMax >= ey * 2.2f &&
			horizontalMin >= ey * 1.25f;

		struct ExpandedVertex {
			BumpMesh::Vertex vertex;
			unsigned int subMeshId = 0;
		};

		struct TriangleRef {
			unsigned int v[3];
			unsigned int subMeshId = 0;
		};

		std::vector<ExpandedVertex> expandedVertices;
		std::vector<unsigned int> expandedIndices;
		std::vector<int> sourceSubMeshTriangleCounts(subMeshCount, 0);
		expandedVertices.reserve(sourceMesh->sourceIndexData.size() * 3);
		expandedIndices.reserve(sourceMesh->sourceIndexData.size() * 3);

		for (size_t subMeshIndex = 0; subMeshIndex + 1 < sourceMesh->sourceSubMeshIndexStart.size(); ++subMeshIndex) {
			const int startIndex = sourceMesh->sourceSubMeshIndexStart[subMeshIndex];
			const int endIndex = sourceMesh->sourceSubMeshIndexStart[subMeshIndex + 1];
			const int subsetIndexCount = endIndex - startIndex;
			if (subsetIndexCount <= 0 || (subsetIndexCount % 3) != 0) continue;

			std::unordered_map<unsigned int, unsigned int> localVertexRemap;
			localVertexRemap.reserve(static_cast<size_t>(subsetIndexCount));

			const int startTriangle = startIndex / 3;
			const int triangleCount = subsetIndexCount / 3;
			sourceSubMeshTriangleCounts[subMeshIndex] = triangleCount;
			for (int tri = 0; tri < triangleCount; ++tri) {
				const TriangleIndex& srcTri = sourceMesh->sourceIndexData[startTriangle + tri];
				for (int corner = 0; corner < 3; ++corner) {
					const unsigned int sourceVertexIndex = srcTri.v[corner];
					auto it = localVertexRemap.find(sourceVertexIndex);
					unsigned int expandedIndex = 0;
					if (it == localVertexRemap.end()) {
						expandedIndex = static_cast<unsigned int>(expandedVertices.size());
						localVertexRemap[sourceVertexIndex] = expandedIndex;
						expandedVertices.push_back({ sourceMesh->sourceVertexData[sourceVertexIndex], static_cast<unsigned int>(subMeshIndex) });
					}
					else {
						expandedIndex = it->second;
					}
					expandedIndices.push_back(expandedIndex);
				}
			}
		}

		if (expandedVertices.empty() || expandedIndices.empty() || (expandedIndices.size() % 3) != 0) {
			gAutoLODSkipReason = "empty-expanded";
			return false;
		}

		const size_t expandedTriangleCount = expandedIndices.size() / 3;
		if (expandedTriangleCount < static_cast<size_t>(kAutoLODMinSubsetTriangles)) {
			gAutoLODSkipReason = "too-few-triangles";
			return false;
		}

		const bool smallSinglePartMesh =
			subMeshCount == 1 &&
			expandedTriangleCount <= 384 &&
			maxExt <= 0.12f;
		const bool largeSinglePartVehicleLike =
			subMeshCount == 1 &&
			expandedTriangleCount >= 128 &&
			(
				(lowProfileWideMesh && maxExt >= 0.15f) ||
				(maxExt >= 0.45f && horizontalMin >= 0.08f && ey <= maxExt * 0.85f) ||
				(maxExt >= 0.90f && horizontalMin >= 0.18f)
			);

		int substantialSubmeshCount = 0;
		for (int triCount : sourceSubMeshTriangleCounts) {
			if (triCount >= 48) {
				++substantialSubmeshCount;
			}
		}

		if (lowProfileWideMesh && substantialSubmeshCount >= 2) {
			gAutoLODSkipReason = "vehicle-structure";
			return false;
		}

		if (largeSinglePartVehicleLike) {
			gAutoLODSkipReason = "vehicle-part-structure";
			return false;
		}

		if (subMeshCount >= 4 && substantialSubmeshCount >= 3) {
			gAutoLODSkipReason = "assembly-structure";
			return false;
		}

		std::vector<AutoLODAttr> attrs(expandedVertices.size());
		for (size_t i = 0; i < expandedVertices.size(); ++i) {
			const BumpMesh::Vertex& v = expandedVertices[i].vertex;
			attrs[i] = { v.normal.x, v.normal.y, v.normal.z, v.u, v.v, v.tangent.x, v.tangent.y, v.tangent.z };
		}

		std::vector<unsigned int> positionRemap(expandedVertices.size());
		meshopt_generateVertexRemapCustom(
			positionRemap.data(),
			expandedIndices.data(),
			expandedIndices.size(),
			&expandedVertices[0].vertex.position.x,
			expandedVertices.size(),
			sizeof(ExpandedVertex),
			[&expandedVertices](unsigned int lhs, unsigned int rhs) {
				constexpr float tol = kAutoLODPositionRemapTolerance;
				const ExpandedVertex& a = expandedVertices[lhs];
				const ExpandedVertex& b = expandedVertices[rhs];
				return fabsf(a.vertex.position.x - b.vertex.position.x) <= tol &&
					fabsf(a.vertex.position.y - b.vertex.position.y) <= tol &&
					fabsf(a.vertex.position.z - b.vertex.position.z) <= tol;
			});

		std::vector<unsigned char> vertexLocks(expandedVertices.size(), 0);
		for (size_t i = 0; i < expandedVertices.size(); ++i) {
			const unsigned int remapIndex = positionRemap[i];
			if (remapIndex >= expandedVertices.size() || remapIndex == i) continue;

			const ExpandedVertex& current = expandedVertices[i];
			const ExpandedVertex& canonical = expandedVertices[remapIndex];
			const RemapPairPolicy policy = GetRemapPairPolicy(current.vertex, current.subMeshId, canonical.vertex, canonical.subMeshId);
			if (policy == RemapPairPolicy::Protect) {
				vertexLocks[i] |= meshopt_SimplifyVertex_Protect;
				vertexLocks[remapIndex] |= meshopt_SimplifyVertex_Protect;
			}
			else if (policy == RemapPairPolicy::Priority) {
				vertexLocks[i] |= meshopt_SimplifyVertex_Priority;
				vertexLocks[remapIndex] |= meshopt_SimplifyVertex_Priority;
			}
		}

		size_t protectedVertexCount = 0;
		for (unsigned char lock : vertexLocks) {
			if ((lock & meshopt_SimplifyVertex_Protect) != 0) {
				++protectedVertexCount;
			}
		}

		if (!vertexLocks.empty() &&
			protectedVertexCount == vertexLocks.size() &&
			(float(protectedVertexCount) / float(vertexLocks.size())) > kAutoLODMaxProtectedVertexRatio) {
			// If every remapped vertex is protected, the seam classifier has become
			// overly conservative for this mesh. Demote hard locks to priority so
			// simplification can still proceed instead of skipping the mesh entirely.
			for (unsigned char& lock : vertexLocks) {
				lock &= ~meshopt_SimplifyVertex_Protect;
				lock |= meshopt_SimplifyVertex_Priority;
			}
			protectedVertexCount = 0;
		}

		float uvAreaSum = 0.0f;
		size_t uvAreaCount = 0;
		for (size_t i = 0; i + 2 < expandedIndices.size(); i += 3) {
			const BumpMesh::Vertex& a = expandedVertices[expandedIndices[i + 0]].vertex;
			const BumpMesh::Vertex& b = expandedVertices[expandedIndices[i + 1]].vertex;
			const BumpMesh::Vertex& c = expandedVertices[expandedIndices[i + 2]].vertex;

			const float uvArea = fabsf(
				(b.u - a.u) * (c.v - a.v) -
				(b.v - a.v) * (c.u - a.u)) * 0.5f;

			if (uvArea > 1e-8f) {
				uvAreaSum += uvArea;
				++uvAreaCount;
			}
		}

		float uvWeight = 1.0f;
		if (uvAreaCount > 0) {
			const float averageUvArea = uvAreaSum / float(uvAreaCount);
			uvWeight = 1.0f / sqrtf((std::max)(averageUvArea, 1e-8f));
			uvWeight = std::clamp(uvWeight, 1.0f, 32.0f);
		}

		const float attrWeights[8] = { 0.75f, 0.75f, 0.75f, uvWeight, uvWeight, 0.25f, 0.25f, 0.25f };
		size_t targetIndexCount = static_cast<size_t>(float(expandedIndices.size()) * kAutoLODRatio);
		targetIndexCount = (targetIndexCount / 3) * 3;
		targetIndexCount = std::max<size_t>(targetIndexCount, static_cast<size_t>(kAutoLODMinResultTriangles * 3));

		float protectedRatio = float(protectedVertexCount) / float((std::max)(vertexLocks.size(), size_t(1)));
		const bool aggressiveSinglePartUnlock =
			subMeshCount == 1 &&
			smallSinglePartMesh &&
			protectedRatio >= 0.85f;
		if (aggressiveSinglePartUnlock) {
			for (unsigned char& lock : vertexLocks) {
				if ((lock & meshopt_SimplifyVertex_Protect) != 0) {
					lock &= ~meshopt_SimplifyVertex_Protect;
					lock |= meshopt_SimplifyVertex_Priority;
				}
			}
			protectedVertexCount = 0;
			protectedRatio = 0.0f;
		}

		const bool isAssemblyLike =
			(subMeshCount >= 4 && expandedTriangleCount >= 512) ||
			(subMeshCount > 1 && protectedRatio >= 0.12f);
		const bool preferClassicSimplifier =
			isAssemblyLike ||
			expandedTriangleCount >= 4096;
		const bool allowPrune =
			!isAssemblyLike &&
			subMeshCount <= 6 &&
			expandedTriangleCount <= 4096 &&
			protectedRatio < 0.20f;

		auto trySimplify = [&](const std::vector<unsigned char>& locks, float targetError, std::vector<unsigned int>& outIndices, std::vector<float>& outPositions, std::vector<AutoLODAttr>& outAttrs, float& outError) -> size_t
		{
			outIndices = expandedIndices;
			outPositions.resize(expandedVertices.size() * 3);
			for (size_t i = 0; i < expandedVertices.size(); ++i) {
				const XMFLOAT3& p = expandedVertices[i].vertex.position;
				outPositions[i * 3 + 0] = p.x;
				outPositions[i * 3 + 1] = p.y;
				outPositions[i * 3 + 2] = p.z;
			}

			outAttrs = attrs;
			outError = 0.0f;
			unsigned int simplifyOptions = meshopt_SimplifyPermissive;
			if (allowPrune) {
				simplifyOptions |= meshopt_SimplifyPrune;
			}

			return meshopt_simplifyWithUpdate(
				outIndices.data(),
				outIndices.size(),
				outPositions.data(),
				expandedVertices.size(),
				sizeof(float) * 3,
				&outAttrs[0].nx,
				sizeof(AutoLODAttr),
				attrWeights,
				8,
				locks.empty() ? nullptr : locks.data(),
				targetIndexCount,
				targetError,
				simplifyOptions,
				&outError);
		};

		auto trySimplifyClassic = [&](const std::vector<unsigned char>& locks, float targetError, std::vector<unsigned int>& outIndices, float& outError) -> size_t
		{
			outIndices = expandedIndices;
			outError = 0.0f;
			unsigned int simplifyOptions = meshopt_SimplifyPermissive;
			if (allowPrune && !preferClassicSimplifier) {
				simplifyOptions |= meshopt_SimplifyPrune;
			}

			return meshopt_simplifyWithAttributes(
				outIndices.data(),
				expandedIndices.data(),
				expandedIndices.size(),
				&expandedVertices[0].vertex.position.x,
				expandedVertices.size(),
				sizeof(ExpandedVertex),
				&attrs[0].nx,
				sizeof(AutoLODAttr),
				attrWeights,
				8,
				locks.empty() ? nullptr : locks.data(),
				targetIndexCount,
				targetError,
				simplifyOptions,
				&outError);
		};

		std::vector<unsigned int> lodIndices;
		std::vector<float> updatedPositions;
		std::vector<AutoLODAttr> updatedAttrs;
		float lodError = 0.0f;
		bool usedUpdatedVertices = !preferClassicSimplifier;
		size_t simplifiedCount = 0;
		const float relaxedTargetError = kAutoLODTargetError * 8.0f;
		const float aggressiveTargetError = kAutoLODTargetError * 32.0f;

		if (preferClassicSimplifier) {
			simplifiedCount = trySimplifyClassic(vertexLocks, kAutoLODTargetError, lodIndices, lodError);
		}
		else {
			simplifiedCount = trySimplify(vertexLocks, kAutoLODTargetError, lodIndices, updatedPositions, updatedAttrs, lodError);
		}

		const size_t minAcceptedIndexCount = std::max<size_t>(
			static_cast<size_t>(kAutoLODMinResultTriangles * 3),
			static_cast<size_t>(float(expandedIndices.size()) * kAutoLODMinAcceptedSubsetRatio));

		auto simplifyAccepted = [&](size_t indexCount) -> bool
		{
			return indexCount >= minAcceptedIndexCount &&
				indexCount < expandedIndices.size() &&
				(indexCount % 3) == 0;
		};

		if (!simplifyAccepted(simplifiedCount)) {
			std::vector<unsigned char> protectOnlyLocks = vertexLocks;
			for (unsigned char& lock : protectOnlyLocks) {
				lock &= ~meshopt_SimplifyVertex_Priority;
			}
			if (preferClassicSimplifier) {
				simplifiedCount = trySimplifyClassic(protectOnlyLocks, kAutoLODTargetError, lodIndices, lodError);
			}
			else {
				simplifiedCount = trySimplify(protectOnlyLocks, kAutoLODTargetError, lodIndices, updatedPositions, updatedAttrs, lodError);
			}
		}

		if (!simplifyAccepted(simplifiedCount) && !isAssemblyLike) {
			const std::vector<unsigned char> noLocks;
			if (preferClassicSimplifier) {
				simplifiedCount = trySimplifyClassic(noLocks, kAutoLODTargetError, lodIndices, lodError);
			}
			else {
				simplifiedCount = trySimplify(noLocks, kAutoLODTargetError, lodIndices, updatedPositions, updatedAttrs, lodError);
			}
		}

		if (!simplifyAccepted(simplifiedCount) && preferClassicSimplifier && !isAssemblyLike) {
			usedUpdatedVertices = true;
			simplifiedCount = trySimplify(vertexLocks, kAutoLODTargetError, lodIndices, updatedPositions, updatedAttrs, lodError);
		}

		if (!simplifyAccepted(simplifiedCount) && preferClassicSimplifier && !isAssemblyLike) {
			std::vector<unsigned char> protectOnlyLocks = vertexLocks;
			for (unsigned char& lock : protectOnlyLocks) {
				lock &= ~meshopt_SimplifyVertex_Priority;
			}
			simplifiedCount = trySimplify(protectOnlyLocks, kAutoLODTargetError, lodIndices, updatedPositions, updatedAttrs, lodError);
		}

		if (!simplifyAccepted(simplifiedCount) && preferClassicSimplifier && !isAssemblyLike) {
			const std::vector<unsigned char> noLocks;
			simplifiedCount = trySimplify(noLocks, kAutoLODTargetError, lodIndices, updatedPositions, updatedAttrs, lodError);
		}

		if (!simplifyAccepted(simplifiedCount) && smallSinglePartMesh) {
			if (preferClassicSimplifier) {
				simplifiedCount = trySimplifyClassic(vertexLocks, relaxedTargetError, lodIndices, lodError);
			}
			else {
				simplifiedCount = trySimplify(vertexLocks, relaxedTargetError, lodIndices, updatedPositions, updatedAttrs, lodError);
			}
		}

		if (!simplifyAccepted(simplifiedCount) && smallSinglePartMesh) {
			const std::vector<unsigned char> noLocks;
			usedUpdatedVertices = true;
			simplifiedCount = trySimplify(noLocks, aggressiveTargetError, lodIndices, updatedPositions, updatedAttrs, lodError);
		}

		const bool allowSloppyFallback =
			subMeshCount <= 4 &&
			expandedTriangleCount <= 2048 &&
			protectedRatio < 0.15f;

		if (!simplifyAccepted(simplifiedCount) && allowSloppyFallback) {
			lodIndices = expandedIndices;
			lodError = 0.0f;
			simplifiedCount = meshopt_simplifySloppy(
				lodIndices.data(),
				expandedIndices.data(),
				expandedIndices.size(),
				&expandedVertices[0].vertex.position.x,
				expandedVertices.size(),
				sizeof(ExpandedVertex),
				targetIndexCount,
				kAutoLODTargetError,
				&lodError);
			usedUpdatedVertices = false;
		}

		if (!simplifyAccepted(simplifiedCount)) {
			if (expandedTriangleCount >= 128 && expandedTriangleCount <= 12000) {
				char dbg[512];
				sprintf_s(dbg,
					"[AutoLOD] simplify-failed-detail hash=%016llx tris=%llu submeshes=%llu substantial=%d protected=%.3f ext=(%.3f,%.3f,%.3f) lowProfile=%d assembly=%d preferClassic=%d allowPrune=%d allowSloppy=%d unlockedSingle=%d acceptedMin=%llu got=%llu\n",
					static_cast<unsigned long long>(BuildMeshHash(sourceMesh)),
					static_cast<unsigned long long>(expandedTriangleCount),
					static_cast<unsigned long long>(subMeshCount),
					substantialSubmeshCount,
					protectedRatio,
					ex,
					ey,
					ez,
					lowProfileWideMesh ? 1 : 0,
					isAssemblyLike ? 1 : 0,
					preferClassicSimplifier ? 1 : 0,
					allowPrune ? 1 : 0,
					allowSloppyFallback ? 1 : 0,
					aggressiveSinglePartUnlock ? 1 : 0,
					static_cast<unsigned long long>(minAcceptedIndexCount / 3),
					static_cast<unsigned long long>(simplifiedCount / 3));
				OutputDebugStringA(dbg);
			}
			gAutoLODSkipReason = "simplify-failed";
			return false;
		}

		if (usedUpdatedVertices) {
			for (size_t i = 0; i < expandedVertices.size(); ++i) {
				BumpMesh::Vertex& v = expandedVertices[i].vertex;
				const AutoLODAttr& attr = updatedAttrs[i];
				v.position = XMFLOAT3(
					updatedPositions[i * 3 + 0],
					updatedPositions[i * 3 + 1],
					updatedPositions[i * 3 + 2]);
				v.normal = XMFLOAT3(attr.nx, attr.ny, attr.nz);
				v.u = attr.u;
				v.v = attr.v;
				v.tangent = XMFLOAT3(attr.tx, attr.ty, attr.tz);
				Normalize3(v.normal);
				Normalize3(v.tangent);
			}
		}

		lodIndices.resize(simplifiedCount);
		meshopt_optimizeVertexCache(lodIndices.data(), lodIndices.data(), lodIndices.size(), expandedVertices.size());

		std::vector<ExpandedVertex> compactVertices = expandedVertices;
		const size_t compactVertexCount = meshopt_optimizeVertexFetch(
			compactVertices.data(),
			lodIndices.data(),
			lodIndices.size(),
			compactVertices.data(),
			compactVertices.size(),
			sizeof(ExpandedVertex));
		compactVertices.resize(compactVertexCount);

		std::vector<std::vector<TriangleRef>> trianglesBySubMesh(sourceMesh->sourceSubMeshIndexStart.size() - 1);
		for (size_t i = 0; i + 2 < lodIndices.size(); i += 3) {
			const unsigned int i0 = lodIndices[i + 0];
			const unsigned int i1 = lodIndices[i + 1];
			const unsigned int i2 = lodIndices[i + 2];
			const unsigned int s0 = compactVertices[i0].subMeshId;
			const unsigned int s1 = compactVertices[i1].subMeshId;
			const unsigned int s2 = compactVertices[i2].subMeshId;

			unsigned int triSubMesh = s0;
			if (s1 == s2) triSubMesh = s1;
			else if (s0 != s1 && s0 != s2) triSubMesh = s1;

			if (triSubMesh >= trianglesBySubMesh.size()) {
				triSubMesh = 0;
			}

			trianglesBySubMesh[triSubMesh].push_back({ { i0, i1, i2 }, triSubMesh });
		}

		std::vector<unsigned int> orderedIndices;
		orderedIndices.reserve(lodIndices.size());
		std::vector<int> orderedSubMeshStarts;
		orderedSubMeshStarts.reserve(trianglesBySubMesh.size() + 1);
		orderedSubMeshStarts.push_back(0);

		for (size_t subMeshIndex = 0; subMeshIndex < trianglesBySubMesh.size(); ++subMeshIndex) {
			for (const TriangleRef& tri : trianglesBySubMesh[subMeshIndex]) {
				orderedIndices.push_back(tri.v[0]);
				orderedIndices.push_back(tri.v[1]);
				orderedIndices.push_back(tri.v[2]);
			}
			orderedSubMeshStarts.push_back(static_cast<int>(orderedIndices.size()));
		}

		if (isAssemblyLike) {
			int degradedLargeSubmeshes = 0;
			for (size_t subMeshIndex = 0; subMeshIndex < trianglesBySubMesh.size(); ++subMeshIndex) {
				const int sourceTriangles = sourceSubMeshTriangleCounts[subMeshIndex];
				const int outputTriangles = static_cast<int>(trianglesBySubMesh[subMeshIndex].size());
				if (sourceTriangles < 48) {
					continue;
				}

				const int minRetainedTriangles = (std::max)(12, sourceTriangles / 5);
				if (outputTriangles < minRetainedTriangles) {
					++degradedLargeSubmeshes;
				}
			}

			if (degradedLargeSubmeshes > 0) {
				gAutoLODSkipReason = "lost-submeshes";
				return false;
			}
		}

		if (orderedIndices.empty()) {
			gAutoLODSkipReason = "empty-output";
			return false;
		}

		meshopt_optimizeVertexCache(orderedIndices.data(), orderedIndices.data(), orderedIndices.size(), compactVertices.size());
		const size_t finalVertexCount = meshopt_optimizeVertexFetch(
			compactVertices.data(),
			orderedIndices.data(),
			orderedIndices.size(),
			compactVertices.data(),
			compactVertices.size(),
			sizeof(ExpandedVertex));
		compactVertices.resize(finalVertexCount);

		outData.vertices.clear();
		outData.vertices.reserve(compactVertices.size());
		for (const ExpandedVertex& v : compactVertices) {
			outData.vertices.push_back(v.vertex);
		}

		outData.indices.clear();
		outData.indices.reserve(orderedIndices.size() / 3);
		for (size_t i = 0; i + 2 < orderedIndices.size(); i += 3) {
			outData.indices.push_back(TriangleIndex(
				orderedIndices[i + 0],
				orderedIndices[i + 1],
				orderedIndices[i + 2]));
		}

		outData.subMeshIndexStart = std::move(orderedSubMeshStarts);
		if (outData.vertices.empty() || outData.indices.empty() || outData.indices.size() >= sourceMesh->sourceIndexData.size()) {
			gAutoLODSkipReason = "not-beneficial";
			return false;
		}

		return true;
	}

	BumpMesh* CreateLODMeshFromData(const BumpMesh* sourceMesh, const AutoLODMeshData& data)
	{
		if (data.vertices.empty() || data.indices.empty() || data.subMeshIndexStart.size() < 2) return nullptr;

		BumpMesh* lodMesh = new BumpMesh();
		lodMesh->IsAutoLODGenerated = true;
		lodMesh->OBB_Ext = sourceMesh->OBB_Ext;
		lodMesh->OBB_Tr = sourceMesh->OBB_Tr;

		int* subMeshStarts = new int[data.subMeshIndexStart.size()];
		for (size_t i = 0; i < data.subMeshIndexStart.size(); ++i) {
			subMeshStarts[i] = data.subMeshIndexStart[i];
		}

		std::vector<BumpMesh::Vertex> vertices = data.vertices;
		std::vector<TriangleIndex> indices = data.indices;
		lodMesh->CreateMesh_FromVertexAndIndexData(vertices, indices, static_cast<int>(data.subMeshIndexStart.size() - 1), subMeshStarts, false);
		gAutoLODOwnedMeshes.push_back(lodMesh);
		return lodMesh;
	}

	void DebugBuildMessage(const char* tag, const std::filesystem::path& path, size_t triCount, const char* detail = nullptr)
	{
		char dbg[512];
		if (detail != nullptr && detail[0] != '\0') {
			sprintf_s(dbg, "[AutoLOD] %s %s tris=%llu reason=%s\n", tag, path.string().c_str(), static_cast<unsigned long long>(triCount), detail);
		}
		else {
			sprintf_s(dbg, "[AutoLOD] %s %s tris=%llu\n", tag, path.string().c_str(), static_cast<unsigned long long>(triCount));
		}
		OutputDebugStringA(dbg);
	}

	float ComputeMeshPriority(const BumpMesh* mesh)
	{
		if (mesh == nullptr) return 0.0f;
		const XMFLOAT3 ext = mesh->OBB_Ext;
		const float ex = (ext.x > 0.0f) ? ext.x : 0.0f;
		const float ey = (ext.y > 0.0f) ? ext.y : 0.0f;
		const float ez = (ext.z > 0.0f) ? ext.z : 0.0f;
		return ex * ey * ez;
	}

	int GetAutoLODPlannedTotal()
	{
		return gAutoLODPreloadTotal;
	}

	int GetAutoLODProcessedTotal()
	{
		return gAutoLODPreloadProcessed + gAutoLODRuntimeProcessed;
	}

	int GetAutoLODUsableTotal()
	{
		return gAutoLODCacheHits + gAutoLODBuilds;
	}

	float GetPercent(int part, int total)
	{
		if (total <= 0) return 0.0f;
		return 100.0f * float(part) / float(total);
	}

	float Dot3(const XMFLOAT3& a, const XMFLOAT3& b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	void Normalize3(XMFLOAT3& v)
	{
		const float lengthSq = Dot3(v, v);
		if (lengthSq <= 1e-8f) return;
		const float invLength = 1.0f / sqrtf(lengthSq);
		v.x *= invLength;
		v.y *= invLength;
		v.z *= invLength;
	}

	RemapPairPolicy GetRemapPairPolicy(const BumpMesh::Vertex& a, unsigned int aSubMeshId, const BumpMesh::Vertex& b, unsigned int bSubMeshId)
	{
		const bool uvDifferent =
			fabsf(a.u - b.u) > kAutoLODUVProtectEpsilon ||
			fabsf(a.v - b.v) > kAutoLODUVProtectEpsilon;
		const float normalDot = Dot3(a.normal, b.normal);
		const float tangentDot = Dot3(a.tangent, b.tangent);

		if (uvDifferent || normalDot < kAutoLODNormalProtectDot || tangentDot < kAutoLODTangentProtectDot) {
			return RemapPairPolicy::Protect;
		}

		if (aSubMeshId != bSubMeshId) {
			if (normalDot >= kAutoLODSubMeshPriorityNormalDot && tangentDot >= kAutoLODSubMeshPriorityTangentDot) {
				return RemapPairPolicy::None;
			}
			if (normalDot >= kAutoLODNormalPriorityDot && tangentDot >= kAutoLODTangentPriorityDot) {
				return RemapPairPolicy::Priority;
			}
			return RemapPairPolicy::None;
		}

		if (normalDot < kAutoLODNormalPriorityDot || tangentDot < kAutoLODTangentPriorityDot) {
			return RemapPairPolicy::Priority;
		}

		return RemapPairPolicy::None;
	}

	const char* GetAutoLODSkipReason()
	{
		return gAutoLODSkipReason ? gAutoLODSkipReason : "unknown";
	}

	void BuildLODForMesh(BumpMesh* mesh)
	{
		if (mesh == nullptr || mesh->IsAutoLODGenerated) return;
		if (!mesh->sourceAutoLODReady || mesh->sourceVertexData.empty() || mesh->sourceIndexData.empty()) {
			++gAutoLODSkips;
			return;
		}
		if (gAutoLODMeshes.find(mesh) != gAutoLODMeshes.end()) return;

		const std::filesystem::path cachePath = GetCachePath(mesh);
		AutoLODCacheHeader header;
		AutoLODMeshData data;

		if (LoadCacheFile(cachePath, header, data)) {
			BumpMesh* lodMesh = CreateLODMeshFromData(mesh, data);
			if (lodMesh != nullptr) {
				gAutoLODMeshes[mesh] = lodMesh;
				++gAutoLODCacheHits;
				DebugBuildMessage("cache-hit", cachePath, data.indices.size());
				return;
			}
		}

		++gAutoLODCacheMisses;
		if (!BuildSimplifiedMeshData(mesh, data)) {
			++gAutoLODSkips;
			DebugBuildMessage("skip", cachePath, mesh->sourceIndexData.size(), GetAutoLODSkipReason());
			return;
		}

		SaveCacheFile(cachePath, mesh, data);
		BumpMesh* lodMesh = CreateLODMeshFromData(mesh, data);
		if (lodMesh != nullptr) {
			gAutoLODMeshes[mesh] = lodMesh;
			++gAutoLODBuilds;
			DebugBuildMessage("build", cachePath, data.indices.size());
		}
	}
}

void AutoLOD_RegisterBumpMesh(BumpMesh* mesh)
{
	if (mesh == nullptr || mesh->IsAutoLODGenerated) return;
	gRegisteredSourceMeshes[mesh] = mesh;
}

void AutoLOD_ResetPreloadQueue()
{
	gPreloadQueue.clear();
	gRuntimeQueue.clear();
	gQueuedPreloadMeshes.clear();
	gAutoLODPreloadProcessed = 0;
	gAutoLODPreloadTotal = 0;
	gAutoLODRuntimeProcessed = 0;
	gAutoLODRuntimeTotal = 0;
}

void AutoLOD_QueueMeshForPreload(Mesh* mesh)
{
	if (mesh == nullptr || mesh->type != Mesh::_BumpMesh) return;

	BumpMesh* bumpMesh = static_cast<BumpMesh*>(mesh);
	if (bumpMesh->IsAutoLODGenerated) return;
	if (gQueuedPreloadMeshes.find(mesh) != gQueuedPreloadMeshes.end()) return;

	gQueuedPreloadMeshes.insert(mesh);
	gPreloadQueue.push_back({ bumpMesh, ComputeMeshPriority(bumpMesh) });
}

void AutoLOD_QueueModelForPreload(Model* model)
{
	if (model == nullptr) return;
	for (int i = 0; i < model->mNumMeshes; ++i) {
		AutoLOD_QueueMeshForPreload(model->mMeshes[i]);
	}
}

void AutoLOD_ProcessPreloadQueue()
{
	if (gPreloadQueue.empty()) return;

	std::sort(gPreloadQueue.begin(), gPreloadQueue.end(), [](const PreloadEntry& a, const PreloadEntry& b) {
		return a.priority > b.priority;
		});

	gAutoLODPreloadProcessed = 0;
	gAutoLODPreloadTotal = static_cast<int>(gPreloadQueue.size());
	gAutoLODRuntimeProcessed = 0;
	gAutoLODRuntimeTotal = 0;

	for (const PreloadEntry& entry : gPreloadQueue) {
		if (entry.priority >= kAutoLODPreloadLargeVolume) {
			++gAutoLODPreloadProcessed;
			BuildLODForMesh(entry.mesh);

			if ((gAutoLODPreloadProcessed % 32) == 0 || gAutoLODPreloadProcessed == gAutoLODPreloadTotal) {
				char dbg[256];
				sprintf_s(dbg,
					"[AutoLOD] preload=%d/%d (%.1f%%) hits=%d builds=%d misses=%d skips=%d\n",
					gAutoLODPreloadProcessed,
					gAutoLODPreloadTotal,
					(gAutoLODPreloadTotal > 0) ? (100.0f * float(gAutoLODPreloadProcessed) / float(gAutoLODPreloadTotal)) : 0.0f,
					gAutoLODCacheHits,
					gAutoLODBuilds,
					gAutoLODCacheMisses,
					gAutoLODSkips);
				OutputDebugStringA(dbg);
			}
		}
		else {
			gRuntimeQueue.push_back(entry);
			++gAutoLODRuntimeTotal;
		}
	}

	if (!gRuntimeQueue.empty()) {
		char dbg[256];
		sprintf_s(dbg,
			"[AutoLOD] deferred=%d small meshes will build during gameplay\n",
			static_cast<int>(gRuntimeQueue.size()));
		OutputDebugStringA(dbg);
	}

	gPreloadQueue.clear();
	gQueuedPreloadMeshes.clear();
}

void AutoLOD_ProcessRuntimeQueue(int maxCount)
{
	if (gRuntimeQueue.empty()) return;
	if (maxCount <= 0) {
		maxCount = kAutoLODRuntimeBuildsPerTick;
	}

	int processedThisTick = 0;
	while (!gRuntimeQueue.empty() && processedThisTick < maxCount) {
		PreloadEntry entry = gRuntimeQueue.back();
		gRuntimeQueue.pop_back();
		++processedThisTick;
		++gAutoLODRuntimeProcessed;
		BuildLODForMesh(entry.mesh);
	}

	if (processedThisTick > 0) {
			const int plannedTotal = GetAutoLODPlannedTotal();
			const int processedTotal = GetAutoLODProcessedTotal();
			const int usableTotal = GetAutoLODUsableTotal();
			char dbg[320];
			sprintf_s(dbg,
				"[AutoLOD] runtime-build=%d/%d remaining=%d progress=%d/%d (%.1f%%) usable=%d/%d (%.1f%%) hits=%d builds=%d misses=%d skips=%d\n",
				gAutoLODRuntimeProcessed,
				gAutoLODRuntimeTotal,
				static_cast<int>(gRuntimeQueue.size()),
				processedTotal,
				plannedTotal,
				GetPercent(processedTotal, plannedTotal),
				usableTotal,
				plannedTotal,
				GetPercent(usableTotal, plannedTotal),
				gAutoLODCacheHits,
				gAutoLODBuilds,
				gAutoLODCacheMisses,
				gAutoLODSkips);
			OutputDebugStringA(dbg);

			if (gRuntimeQueue.empty()) {
				char doneDbg[256];
				sprintf_s(doneDbg,
					"[AutoLOD] generation-complete progress=%d/%d (%.1f%%) usable=%d/%d (%.1f%%) skipped=%d\n",
					processedTotal,
					plannedTotal,
					GetPercent(processedTotal, plannedTotal),
					usableTotal,
					plannedTotal,
					GetPercent(usableTotal, plannedTotal),
					gAutoLODSkips);
				OutputDebugStringA(doneDbg);
			}
	}
}

Mesh* AutoLOD_GetLODMesh(Mesh* sourceMesh)
{
	auto it = gAutoLODMeshes.find(sourceMesh);
	if (it == gAutoLODMeshes.end()) return nullptr;
	return it->second;
}

void AutoLOD_SetModelLODRenderActive(bool active)
{
	gAutoLODModelRenderActive = active;
}

bool AutoLOD_IsModelLODRenderActive()
{
	return gAutoLODModelRenderActive;
}

void AutoLOD_BeginFrame()
{
	gAutoLODTrackFrameStats = true;
	gAutoLODFrameTotal = 0;
	gAutoLODFrameActive = 0;
	gAutoLODFrameResolved = 0;
}

void AutoLOD_RecordFrameSelection(bool usedLOD, bool resolvedLOD)
{
	if (!gAutoLODTrackFrameStats) return;
	++gAutoLODFrameTotal;
	if (usedLOD) ++gAutoLODFrameActive;
	if (resolvedLOD) ++gAutoLODFrameResolved;
}

void AutoLOD_DebugUpdate(float deltaTime)
{
	gAutoLODDebugTimer += deltaTime;
	if (gAutoLODDebugTimer < kAutoLODDebugInterval) return;
	gAutoLODDebugTimer = 0.0f;

	float activePct = 0.0f;
	float resolvedPct = 0.0f;
	if (gAutoLODFrameTotal > 0) {
		activePct = 100.0f * float(gAutoLODFrameActive) / float(gAutoLODFrameTotal);
		resolvedPct = 100.0f * float(gAutoLODFrameResolved) / float(gAutoLODFrameTotal);
	}

	const int plannedTotal = GetAutoLODPlannedTotal();
	const int processedTotal = GetAutoLODProcessedTotal();
	const int usableTotal = GetAutoLODUsableTotal();
	const bool generationComplete = (plannedTotal > 0) && (processedTotal >= plannedTotal) && gRuntimeQueue.empty();

	char dbg[384];
	sprintf_s(dbg,
		"[AutoLOD] active=%d/%d (%.1f%%) resolved=%d (%.1f%%) gen=%d/%d (%.1f%%,%s) usable=%d/%d (%.1f%%) hits=%d builds=%d misses=%d skips=%d ratio=%.2f\n",
		gAutoLODFrameActive,
		gAutoLODFrameTotal,
		activePct,
		gAutoLODFrameResolved,
		resolvedPct,
		processedTotal,
		plannedTotal,
		GetPercent(processedTotal, plannedTotal),
		generationComplete ? "complete" : "building",
		usableTotal,
		plannedTotal,
		GetPercent(usableTotal, plannedTotal),
		gAutoLODCacheHits,
		gAutoLODBuilds,
		gAutoLODCacheMisses,
		gAutoLODSkips,
		kAutoLODRatio);
	OutputDebugStringA(dbg);
}
