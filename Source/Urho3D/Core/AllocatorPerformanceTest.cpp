// 性能测试：检查当前分配器的调用开销
// 临时测试文件，用于分析分配器效率

#include "../Precompiled.h"
#include <chrono>
#include <iostream>
#include "EASTLAllocator.h"
#include <EASTL/allocator.h>

#ifdef URHO3D_HAS_MIMALLOC
#include <mimalloc.h>
#endif

// 测试不同分配器的性能
void TestAllocatorPerformance()
{
    const int NUM_ALLOCS = 100000;
    const size_t SIZE = 1024;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 测试1: 直接mimalloc
#ifdef URHO3D_HAS_MIMALLOC
    std::cout << "Testing direct mimalloc..." << std::endl;
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_ALLOCS; ++i) {
        void* p = mi_malloc(SIZE);
        mi_free(p);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Direct mimalloc: " << duration.count() << " microseconds" << std::endl;
#endif
    
    // 测试2: 通过全局new/delete重载
    std::cout << "Testing global new/delete overrides..." << std::endl;
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_ALLOCS; ++i) {
        void* p = ::operator new(SIZE);
        ::operator delete(p);
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Global new/delete: " << duration.count() << " microseconds" << std::endl;
    
    // 测试3: 通过EASTL分配器
    std::cout << "Testing EASTL allocator..." << std::endl;
    start = std::chrono::high_resolution_clock::now();
    auto allocator = eastl::GetDefaultAllocator();
    for (int i = 0; i < NUM_ALLOCS; ++i) {
        void* p = allocator->allocate(SIZE);
        allocator->deallocate(p, SIZE);
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "EASTL allocator: " << duration.count() << " microseconds" << std::endl;
    
    // 测试4: Urho3D原生容器
    std::cout << "Testing Urho3D containers..." << std::endl;
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_ALLOCS/100; ++i) { // 减少次数，因为Vector操作更重
        Urho3D::Vector<int> vec;
        vec.Reserve(100);
        for (int j = 0; j < 100; ++j) {
            vec.Push(j);
        }
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Urho3D Vector: " << duration.count() << " microseconds" << std::endl;
    
    // 测试5: EASTL容器
    std::cout << "Testing EASTL containers..." << std::endl;
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_ALLOCS/100; ++i) {
        eastl::vector<int> vec;
        vec.reserve(100);
        for (int j = 0; j < 100; ++j) {
            vec.push_back(j);
        }
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "EASTL vector: " << duration.count() << " microseconds" << std::endl;
}

// 测试内存对齐性能
void TestAlignedAllocation()
{
    const int NUM_ALLOCS = 10000;
    const size_t SIZE = 1024;
    const size_t ALIGNMENT = 64; // Cache line alignment
    
    std::cout << "\nTesting aligned allocation..." << std::endl;
    
#ifdef URHO3D_HAS_MIMALLOC
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_ALLOCS; ++i) {
        void* p = mi_malloc_aligned(SIZE, ALIGNMENT);
        mi_free(p);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Direct mimalloc aligned: " << duration.count() << " microseconds" << std::endl;
#endif
    
    // 通过EASTL分配器的对齐分配
    auto start2 = std::chrono::high_resolution_clock::now();
    auto allocator = eastl::GetDefaultAllocator();
    for (int i = 0; i < NUM_ALLOCS; ++i) {
        void* p = allocator->allocate(SIZE, ALIGNMENT, 0);
        allocator->deallocate(p, SIZE);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);
    std::cout << "EASTL aligned allocator: " << duration2.count() << " microseconds" << std::endl;
}

