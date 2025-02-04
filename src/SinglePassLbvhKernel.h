/*MIT License

Copyright (c) 2024 Nirvaana

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.*/

#include <src/Common.h>

using namespace BvhConstruction;

extern "C" __global__ void InitBvhNodes(
	const Triangle* __restrict__ primitives,
	Bvh2Node* __restrict__ bvhNodes,
	const u32* __restrict__ primIdx,
	const u32 nInternalNodes,
	const u32 nLeafNodes)
{
	unsigned int gIdx = threadIdx.x + blockIdx.x * blockDim.x;

	if (gIdx < nLeafNodes)
	{
		const u32 nodeIdx = gIdx + nInternalNodes;
		u32 idx = primIdx[gIdx];
		Bvh2Node& node = bvhNodes[nodeIdx];
		node.m_aabb.reset();
		node.m_aabb.grow(primitives[idx].v1); node.m_aabb.grow(primitives[idx].v2); node.m_aabb.grow(primitives[idx].v3);
		node.m_leftChildIdx = idx;
		node.m_rightChildIdx = INVALID_NODE_IDX;
	}

	if (gIdx < nInternalNodes)
	{
		Bvh2Node& node = bvhNodes[gIdx];
		node.m_aabb.reset();
		node.m_leftChildIdx = INVALID_NODE_IDX;
		node.m_rightChildIdx = INVALID_NODE_IDX;
	}
}

DEVICE uint64_t findHighestDiffBit(const u32* __restrict__ mortonCodes, int i, int j, int n)
{
	if (j < 0 || j >= n) return ~0ull;
	const uint64_t a = (static_cast<uint64_t>(mortonCodes[i]) << 32ull) | i;
	const uint64_t b = (static_cast<uint64_t>(mortonCodes[j]) << 32ull) | j;
	return a ^ b;
}

DEVICE int findParent(
	Bvh2Node* __restrict__ bvhNodes,
	uint2* spans,
	const u32* __restrict__ mortonCodes,
	u32 currentNodeIdx,
	int i,
	int j,
	int n)
{
	if (i == 0 && j == n) return INVALID_NODE_IDX;
	if (i == 0 || (j != n && findHighestDiffBit(mortonCodes, j - 1, j, n) < findHighestDiffBit(mortonCodes, i - 1, i, n)))
	{
		bvhNodes[j - 1].m_leftChildIdx = currentNodeIdx;
		spans[j - 1].x = i;
		return j - 1;
	}
	else
	{
		bvhNodes[i - 1].m_rightChildIdx = currentNodeIdx;
		spans[i - 1].y = j;
		return i - 1;
	}
}

extern "C" __global__ void BvhBuildAndFit(
	Bvh2Node* __restrict__ bvhNodes,
	int* __restrict__ bvhNodeCounter,
	uint2* __restrict__ spans,
	const u32* __restrict__ mortonCodes,
	u32 nLeafNodes,
	u32 nInternalNodes)
{
	int gIdx = blockIdx.x * blockDim.x + threadIdx.x;
	if (gIdx >= nLeafNodes) return;

	int i = gIdx;
	int j = gIdx + 1;

	int parentIdx = findParent(bvhNodes, spans, mortonCodes, nInternalNodes + gIdx, i, j, nLeafNodes);
	gIdx = parentIdx;

	while (atomicAdd(&bvhNodeCounter[gIdx], 1) > 0)
	{
		__threadfence();

		Bvh2Node& node = bvhNodes[gIdx];
		uint2 span = spans[gIdx];

		node.m_aabb = merge(bvhNodes[node.m_leftChildIdx].m_aabb, bvhNodes[node.m_rightChildIdx].m_aabb);

		parentIdx = findParent(bvhNodes, spans, mortonCodes, gIdx, span.x, span.y, nLeafNodes);

		if (parentIdx == INVALID_NODE_IDX)
		{
			bvhNodeCounter[nLeafNodes - 1] = gIdx; //Saving root node;
			break;
		}

		gIdx = parentIdx;

		__threadfence();
	}
}
