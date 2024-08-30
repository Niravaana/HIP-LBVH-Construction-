# Steps to Build and Run

1. git clone the repo
2. git submodule update --init --recursive 
3. Run .\tools\premake5\win\premake5.exe vs2022
4. Build directory will be created which will have the solution file.

# HIP-BVH-Construction

This repo is implementation of different GPU BVH build methods and optimizations. Following methods are already implemented.

1. [LBVH](https://github.qualcomm.com/parikulk/BvhBuilderFramework/blob/e073ed347939bc66b25540a8dff830d512b5138b/src/TwoPassLbvhKernel.h#L174) - Two pass method based on the research paper [Maximizing Parallelism in the Construction of BVHs,Octrees, and k-d Trees](https://research.nvidia.com/sites/default/files/publications/karras2012hpg_paper.pdf) **[DONE]**
2. [LBVH](https://github.qualcomm.com/parikulk/BvhBuilderFramework/blob/e073ed347939bc66b25540a8dff830d512b5138b/src/SinglePassLbvhKernel.h#L67) - Single pass method based on the research paper  [Fast and Simple Agglomerative LBVH Construction](https://diglib.eg.org/server/api/core/bitstreams/ad092db2-6aec-4f2c-941d-8687de258f00/content) **[DONE]**
3. [Inplace LBVH Build](https://github.qualcomm.com/parikulk/BvhBuilderFramework/blob/543a715ac9c66afccb93186034c0b9af9ccd6115/src/BatchedBuildKernel.h#L211) - LBVH is built in single kernel call it takes 0.4 ms to build 2048 bvhs of cornell box on AMD 780M.**[DONE]**
4. [Ploc++](https://github.qualcomm.com/parikulk/BvhBuilderFramework/blob/e073ed347939bc66b25540a8dff830d512b5138b/src/Ploc%2B%2BKernel.h#L77) - Based on the [PLOC++ paper](https://www.intel.com/content/www/us/en/developer/articles/technical/ploc-for-bounding-volume.html) **[DONE]**
5. [Inplace PLOC++ Build](https://github.qualcomm.com/parikulk/BvhBuilderFramework/blob/d26258273c6b95749b6335fe4562a5d2c6d08524/src/Ploc%2B%2BKernel.h#L77) - When nClusters drop below block size we can optimize the PLOC++ further. After this ploc bvh builder takes ~1ms less.**[DONE]**
6. [HPLOC](https://github.qualcomm.com/parikulk/BvhBuilderFramework/blob/b0377b1c22d9f65e80a2f788e2b84722c576aaf3/src/HplocKernel.h#L282) - Based on the [paper](https://gpuopen.com/download/publications/HPLOC.pdf)
7. [Collapse LBVH to nwide BVH](https://github.qualcomm.com/parikulk/BvhBuilderFramework/blob/4ab253f9846f8bb1a979ad87da5cf2d092195107/src/TwoPassLbvhKernel.h#L215) - Based on the research paper [Getting Rid of Packets Efficient SIMD Single-Ray Traversal using Multi-branching BVHs](https://typeset.io/pdf/getting-rid-of-packets-efficient-simd-single-ray-traversal-29rwgbmwv3.pdf). This implementation is modified so that only the child with highest SAH is collapsed resulting in reducing the oversall SAH cost of the widened BVH. **[Cpu/Gpu DONE]**
   The BVH SAH cost for bunny model drops from ~46 to ~22 while for sponza it drops to ~59 from ~131.
5. [Binned SAH Builder](https://github.qualcomm.com/parikulk/BvhBuilderFramework/blob/main/src/BinnedSahBvh.cpp) - Based on research paper [On fast Construction of SAH-based Bounding Volume Hierarchies, by I. Wald](https://www.sci.utah.edu/~wald/Publications/2007/ParallelBVHBuild/fastbuild.pdf). Currently this is CPU only build, though implemented based on task model so should be easy to port to GPU.**[CPU DONE]**
6. [Extended Morton codes](https://github.qualcomm.com/parikulk/BvhBuilderFramework/blob/f45ec0e712943848703ed073dccf28f5fcf257d4/src/CommonBlocksKernel.h#L138) - Based on the paper [Extended Morton Codes](https://www.highperformancegraphics.org/wp-content/uploads/2017/Papers-Session3/HPG207_ExtendedMortonCodes.pdf). **[DONE]**
7. [Early Split Clipping](https://github.qualcomm.com/parikulk/BvhBuilderFramework/blob/4ab253f9846f8bb1a979ad87da5cf2d092195107/src/Utility.cpp#L456) - Based on the research paper [Early Split Clipping for Bounding Volume Hierarchies](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=ddfac027fa516d63fa705c52155ea9313543cf3a). **[DONE]**
   Basic idea is to split the AABB of the primitive along the maximum extent midpoint. The problem is leaf node explosion and the criteria to decide to split is user defined parameter. It is hard to find a generic value for this heuristic. Though,
   I dont see any performance boost in traversal at least for sponza.(Hard to verify my implementation as of now).

# Reference Images

![test](https://github.qualcomm.com/storage/user/34949/files/727f4a4f-f2de-4c57-82e1-62da40337e3a)

![test](https://github.qualcomm.com/storage/user/34949/files/32b92af9-ddaf-413b-8218-4d7f69ae7b8f)


Leaf Node Visits 

![colorMap](https://github.qualcomm.com/storage/user/34949/files/e0cc1945-6cba-4068-bcf7-9fa491e3f522)

![colorMap](https://github.qualcomm.com/storage/user/34949/files/127fc97f-27e7-408f-a393-747fff4a0027)

# Performance Numbers on (AMD 780M)

**Two Pass LBVH Bunny 150K primitives**
1. CalculateCentroidExtentsTime :0.7129ms
2. CalculateMortonCodesTime :0.1607ms
3. SortingTime : 0.6594ms
4. BvhBuildTime : 2.1964ms
5. Total Time : 3.7294ms
6. TraversalTime : 0.3607ms
7. CollapseTime : 3.7827ms
8. Bvh Cost : 22.6398

**Two Pass LBVH Sponza 260K primitives**
1. CalculateCentroidExtentsTime :0.539599ms
2. CalculateMortonCodesTime :0.372499ms
3. SortingTime : 1.4922ms
4. BvhBuildTime : 3.4605ms
5. Total Time : 5.8648ms
6. TraversalTime : 4.6364ms
7. CollapseTime : 6.6873ms
8. Bvh Cost : 59.4779

**Single Pass LBVH Bunny 150K primitives**
1. CalculateCentroidExtentsTime :0.5193ms
2. CalculateMortonCodesTime :0.1653ms
3. SortingTime : 0.6667ms
4. BvhBuildTime : 2.2707ms
5. Total Time : 3.622ms
6. TraversalTime : 0.4707ms
7. CollapseTime : 3.8727ms
8. Bvh Cost : 22.6397

**Single Pass LBVH Sponza 260K primitives**
1. CalculateCentroidExtentsTime :0.499001ms
2. CalculateMortonCodesTime :0.399701ms
3. SortingTime : 1.4701ms
4. BvhBuildTime : 2.9514ms
5. Total Time : 5.3202ms
6. TraversalTime : 0.4041ms
7. CollapseTime : 8.4943ms
8. Bvh Cost : 59.4779

**PLOC++ Bunny 150K primitives(radius 8)**
1. CalculateCentroidExtentsTime :0.5535ms
2. CalculateMortonCodesTime :0.3368ms
3. SortingTime : 0.6769ms
4. BvhBuildTime : 2.0855ms
5. Total Time : 3.6527ms
6. TraversalTime : 0ms
7. CollapseTime : 6.9643ms
8. Bvh Cost : 22.2967

**PLOC++ Sponza 260K primitives(radius 8)**
1. CalculateCentroidExtentsTime :0.8387ms
2. CalculateMortonCodesTime :0.8707ms
3. SortingTime : 1.4994ms
4. BvhBuildTime : 4.0968ms
5. Total Time : 7.3056ms
6. TraversalTime : 0ms
7. CollapseTime : 8.9909ms
8. Bvh Cost : 48.842

**HPLOC Bunny 150K primitives(radius 8)**
1. CalculateCentroidExtentsTime :0.5633ms
2. CalculateMortonCodesTime :0.409ms
3. SortingTime : 0.637ms
4. BvhBuildTime : 1.3881ms
5. Total Time : 2.9974ms
6. TraversalTime : 0ms
7. CollapseTime : 5.1155ms
8. Bvh Cost : 21.9677

**HPLOC Sponza 260K primitives**
1. CalculateCentroidExtentsTime :0.7894ms
2. CalculateMortonCodesTime :0.8681ms
3. SortingTime : 1.9ms
4. BvhBuildTime : 2.3178ms
5. Total Time : 5.8753ms
6. TraversalTime : 0ms
7. CollapseTime : 7.1133ms
8. Bvh Cost : 48.2362

**Batched LBVH Builder Cornell Box 32 primitives**
1. BatchSize : 2048
2. BvhBuildTime to build 2048 bvhs: 0.4813ms

**Note** : we are still calculating cost on Bvh4 tree after Bvh8 collapse the cost is expected to be reduced by ~half.

# Performance Numbers RTLib (AMD 780M) (Subject to errors. Has to be reverified.)
Constructed bvh with 0 restructure passes.
The timings can vary by a couple milliseconds each run.
BVH2 cost is shalf the sum of surface area of all node bounds. The cost varies between runs.

**LBVH Sponza 260K primitives**
1. Triangle vertex buffer repack : 2.7365ms
2. Primitive AABB                : 2.5362ms
3. Scene AABB                    : 0.0151ms
4. Generate Morton Codes         : 0.2218ms
5. Sorting Morton Codes          : 9.6347ms
6. Binary LBVH - build Hierarchy : 1.0838ms
8. Total                         : 15.14ms
10. BVH2 Sah : 111.24
11. BVH8 Sah : 37.16

**LBVH bunny 150K primitives**
1. Triangle vertex buffer repack : 0.4745ms
2. Primitive AABB                : 1.2420ms
3. Scene AABB                    : 0.3343ms
4. Generate Morton Codes         : 0.1032ms
5. Sorting Morton Codes          : 5.2113ms
6. Binary LBVH - build Hierarchy : 0.6137ms
8. Total                         : 7.37ms (observed up to 12.53ms)
10. BVH2 Sah : 45.34
11. BVH8 Sah : 14.14

**LBVH Cornell Box 32 primitives**
CRASH! Have to investigate.

# ToDo
1. Move primitive aabb/ref generation to GPU for better comparison with rtlib and it is required in the driver anyway.
2. Add TLAS building.
3. Compute centroid bounds as well and compute morton codes from that. Check effects on BVH quality.
4. Add inplace LBVH build (useful for batching prims with small prims) **[DONE]**
5. Add inplace PLOC++ builds (useful for batching prims with small prims) **[DONE]**
6. Add HPLOC (HPG 2024)
7. Add batched builder(load multiple cornel boxes and build bvh in one go) **[DONE]**
8. Implement BVH8 collapsing - This should be straighforward extension of current implementation.
9. Change BVH8 node to compressed format takes only 80bytes based on the [paper](https://research.nvidia.com/sites/default/files/publications/ylitie2017hpg-paper.pdf)
10. BVH Refitting and update
11. BVH Compaction - doing DFS in a built BVH and prepare new nodes.
12. Triangle pairing - try to pair triangles and store them in leaf, this might help to reduce number of total nodes and save memory.
13. Explore high quality builders SBVH, Bonsai.
14. Explore triangle pre splitting methods such as described in the TRBVH paper to better assess the benefit.
15. Explore hybrid builders ie, algortihms that build treelets of faster but merges treelets with high quality - HLBVH  
16. Implement ATR - the purpose of implementing this is to understand if it really gives any benefits vs time we spend doing this.
17. Implement TRBVH - the purpose of implementing this is to understand, how similar ATR quality is to TRBVH.
18. Implement other heuristics than surface area like OSAH, ray distribution and explore the EPO and LCV quality metrics for BVHs based on the [paper](https://users.aalto.fi/~laines9/publications/aila2013hpg_paper.pdf)
19. Reduction in memory usage overall.
20. Add BVH serialization so we can load it in DX12 app and use that bvh.
