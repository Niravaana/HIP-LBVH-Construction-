#include <dependencies/Orochi/Orochi/OrochiUtils.h>
#include <dependencies/Orochi/Orochi/GpuMemory.h>
#include <ParallelPrimitives/RadixSort.h>
#include <src/Error.h>
#include <src/Kernel.h>
#include <src/Timer.h>
#include <iostream>
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

using namespace BvhConstruction;

void loadScene(const std::string& filename,	const std::string& mtlBaseDir, std::vector<Triangle>& trianglesOut )
{
	tinyobj::attrib_t				 attrib;
	std::vector<tinyobj::shape_t>	 shapes;
	std::vector<tinyobj::material_t> materials;
	std::string						 err;
	std::string						 warning;

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &err, filename.c_str(), mtlBaseDir.c_str());

	if (!warning.empty())
	{
		std::cerr << "OBJ Loader WARN : " << warning << std::endl;
	}

	if (!err.empty())
	{
		std::cerr << "OBJ Loader ERROR : " << err << std::endl;
		std::exit(EXIT_FAILURE);
	}

	if (!ret)
	{
		std::cerr << "Failed to load obj file" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	if (shapes.empty())
	{
		std::cerr << "No shapes in obj file (run 'git lfs fetch' and 'git lfs pull' in 'test/common/meshes/lfs')" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	std::vector<int>		materialIndices; // material ids for all instances
	std::vector<u32>	instanceMask;
	std::vector<float3>		allVertices;
	std::vector<float3>		allNormals;
	std::vector<u32>	allIndices;

	// Prefix sum to calculate the offsets in to global vert,index and material buffer
	int						 vertexPrefixSum = 0;
	int						 normalPrefixSum = 0;
	int						 indexPrefixSum = 0;
	std::vector<int>		 indicesOffsets;
	std::vector<int>		 verticesOffsets;
	std::vector<int>		 normalsOffsets;

	indicesOffsets.resize(shapes.size());
	verticesOffsets.resize(shapes.size());
	normalsOffsets.resize(shapes.size());

	auto convert = [](const tinyobj::real_t c[3]) -> float4 { return float4{ c[0], c[1], c[2], 0.0f }; };
	
	auto compare = [](const tinyobj::index_t& a, const tinyobj::index_t& b) {
		if (a.vertex_index < b.vertex_index) return true;
		if (a.vertex_index > b.vertex_index) return false;

		if (a.normal_index < b.normal_index) return true;
		if (a.normal_index > b.normal_index) return false;

		if (a.texcoord_index < b.texcoord_index) return true;
		if (a.texcoord_index > b.texcoord_index) return false;

		return false;
	};

	for (size_t i = 0; i < shapes.size(); ++i)
	{
		std::vector<float3>									 vertices;
		std::vector<float3>									 normals;
		std::vector<u32>								 indices;
		float3* v = reinterpret_cast<float3*>(attrib.vertices.data());
		std::map<tinyobj::index_t, int, decltype(compare)> knownIndex(compare);

		for (size_t face = 0; face < shapes[i].mesh.num_face_vertices.size(); face++)
		{
			tinyobj::index_t idx0 = shapes[i].mesh.indices[3 * face + 0];
			tinyobj::index_t idx1 = shapes[i].mesh.indices[3 * face + 1];
			tinyobj::index_t idx2 = shapes[i].mesh.indices[3 * face + 2];

			if (knownIndex.find(idx0) != knownIndex.end())
			{
				indices.push_back(knownIndex[idx0]);
			}
			else
			{
				knownIndex[idx0] = static_cast<int>(vertices.size());
				indices.push_back(knownIndex[idx0]);
				vertices.push_back(v[idx0.vertex_index]);
				normals.push_back(v[idx0.normal_index]);
			}

			if (knownIndex.find(idx1) != knownIndex.end())
			{
				indices.push_back(knownIndex[idx1]);
			}
			else
			{
				knownIndex[idx1] = static_cast<int>(vertices.size());
				indices.push_back(knownIndex[idx1]);
				vertices.push_back(v[idx1.vertex_index]);
				normals.push_back(v[idx1.normal_index]);
			}

			if (knownIndex.find(idx2) != knownIndex.end())
			{
				indices.push_back(knownIndex[idx2]);
			}
			else
			{
				knownIndex[idx2] = static_cast<int>(vertices.size());
				indices.push_back(knownIndex[idx2]);
				vertices.push_back(v[idx2.vertex_index]);
				normals.push_back(v[idx2.normal_index]);
			}

			materialIndices.push_back(shapes[i].mesh.material_ids[face]);
		}

		verticesOffsets[i] = vertexPrefixSum;
		vertexPrefixSum += static_cast<int>(vertices.size());
		indicesOffsets[i] = indexPrefixSum;
		indexPrefixSum += static_cast<int>(indices.size());
		normalsOffsets[i] = normalPrefixSum;
		normalPrefixSum += static_cast<int>(normals.size());

		allVertices.insert(allVertices.end(), vertices.begin(), vertices.end());
		allNormals.insert(allNormals.end(), normals.begin(), normals.end());
		allIndices.insert(allIndices.end(), indices.begin(), indices.end());
	}

	for (size_t i = 0; i < shapes.size(); ++i)
	{
		uint32_t* indices = &allIndices[indicesOffsets[i]];
		float3* vertices = &allVertices[verticesOffsets[i]];
		u32 indexCount = 3 * static_cast<u32>(shapes[i].mesh.num_face_vertices.size());

		for (int j = 0; j < indexCount; j += 3)
		{
			const u32 idx1 = indices[j];
			const u32 idx2 = indices[j + 1];
			const u32 idx3 = indices[j + 2];

			trianglesOut.push_back(Triangle{ vertices[idx1], vertices[idx2], vertices[idx3] });
		}
	}
}

enum TimerCodes
{
	CalculateCentroidExtentsTime,
	CalculateMortonCodesTime,
	SortingTime,
	BvhBuildTime
};

int main(int argc, char* argv[])
{
	try
	{
		oroDevice	orochiDevice;
		oroCtx		orochiCtxt;
		Timer timer;
		
		std::vector<Triangle> triangles;
		loadScene("../src/meshes/cornellbox/cornellbox.obj", "../src/meshes/cornellbox/", triangles);

		CHECK_ORO((oroError)oroInitialize((oroApi)(ORO_API_HIP), 0));
		CHECK_ORO(oroInit(0));
		CHECK_ORO(oroDeviceGet(&orochiDevice, 0)); // deviceId should be taken from user?
		CHECK_ORO(oroCtxCreate(&orochiCtxt, 0, orochiDevice));
		oroDeviceProp props;
		CHECK_ORO(oroGetDeviceProperties(&props, orochiDevice));
		std::cout << "Executing on '" << props.name << "'" << std::endl;

		const u32 primitiveCount = triangles.size();

		Oro::GpuMemory<Triangle> d_triangleBuff(triangles.size()); d_triangleBuff.reset();
		OrochiUtils::copyHtoD(d_triangleBuff.ptr(), triangles.data(), triangles.size());

		Oro::GpuMemory<Aabb> d_centroidExtents(1); d_centroidExtents.reset();
		{
			Kernel centroidExtentsKernel;

			buildKernelFromSrc(
				centroidExtentsKernel,
				orochiDevice,
				"../src/LbvhKernel.h",
				"CalculateCentroidExtents",
				std::nullopt);

			centroidExtentsKernel.setArgs({d_triangleBuff.ptr(), d_centroidExtents.ptr()});
			timer.measure(TimerCodes::CalculateCentroidExtentsTime, [&]() { centroidExtentsKernel.launch(primitiveCount); });
		}

		Oro::GpuMemory<u32> d_mortonCodeKeys(primitiveCount); d_mortonCodeKeys.reset();
		Oro::GpuMemory<u32> d_mortonCodeValues(primitiveCount); d_mortonCodeValues.reset();
		Oro::GpuMemory<u32> d_sortedMortonCodeKeys(primitiveCount); d_sortedMortonCodeKeys.reset();
		Oro::GpuMemory<u32> d_sortedMortonCodeValues(primitiveCount); d_sortedMortonCodeValues.reset();
		{
			Kernel calulateMortonCodesKernel;

			buildKernelFromSrc(
				calulateMortonCodesKernel,
				orochiDevice,
				"../src/LbvhKernel.h",
				"CalculateMortonCodes",
				std::nullopt);

			calulateMortonCodesKernel.setArgs({ d_triangleBuff.ptr(), d_centroidExtents.ptr() , d_mortonCodeKeys.ptr(), d_mortonCodeValues.ptr()});
			timer.measure(TimerCodes::CalculateMortonCodesTime, [&]() { calulateMortonCodesKernel.launch(primitiveCount); });
		}

		{
			OrochiUtils oroUtils;
			Oro::RadixSort sort(orochiDevice, oroUtils);

			Oro::RadixSort::KeyValueSoA srcGpu{};
			Oro::RadixSort::KeyValueSoA dstGpu{};
			static constexpr auto startBit{ 0 };
			static constexpr auto endBit{ 32 };
			static constexpr auto stream = 0;

			srcGpu.key = d_mortonCodeKeys.ptr();
			srcGpu.value = d_mortonCodeValues.ptr();

			dstGpu.key = d_sortedMortonCodeKeys.ptr();
			dstGpu.value = d_sortedMortonCodeValues.ptr();

			timer.measure(SortingTime, [&]() {
				sort.sort(srcGpu, dstGpu, static_cast<int>(primitiveCount), startBit, endBit, stream); });
		}

		//We will store primCount internal nodes followed by leaf nodes
		const u32 nLeafNodes = primitiveCount;
		const u32 nInternalNodes = nLeafNodes - 1;
		Oro::GpuMemory<LbvhInternalNode> d_internalNodes(nInternalNodes);
		Oro::GpuMemory<LbvhLeafNode> d_leafNodes(nLeafNodes);
		{
			{
				Kernel initBvhNodesKernel;

				buildKernelFromSrc(
					initBvhNodesKernel,
					orochiDevice,
					"../src/LbvhKernel.h",
					"InitBvhNodes",
					std::nullopt);

				initBvhNodesKernel.setArgs({ d_internalNodes.ptr(), d_leafNodes.ptr(), nLeafNodes});
				timer.measure(TimerCodes::BvhBuildTime, [&]() { initBvhNodesKernel.launch(nLeafNodes); });
			}

			{
				Kernel bvhBuildKernel;

				buildKernelFromSrc(
					bvhBuildKernel,
					orochiDevice,
					"../src/LbvhKernel.h",
					"bvhBuil",
					std::nullopt);

				bvhBuildKernel.setArgs({ d_internalNodes.ptr(), d_leafNodes.ptr(), d_sortedMortonCodeKeys.ptr(), nLeafNodes, nInternalNodes });
				timer.measure(TimerCodes::BvhBuildTime, [&]() { bvhBuildKernel.launch(nInternalNodes); });
			}
		}

		CHECK_ORO(oroCtxDestroy(orochiCtxt));
	}
	catch (std::exception& e)
	{
		std::cerr << e.what();
		return -1;
	}
	return 0;
}