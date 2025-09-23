// 可选的mimalloc性能调优配置
// 添加到 MemoryHooks.cpp 中

#ifdef URHO3D_HAS_MIMALLOC
void OptimizeMimallocForGame()
{
    // 针对游戏优化的mimalloc配置
    
    // 1. 减少内存占用（如果内存比性能更重要）
    mi_option_set(mi_option_eager_commit, 0);        // 延迟提交内存
    mi_option_set(mi_option_eager_commit_delay, 10); // 10ms延迟
    
    // 2. 针对小对象优化（适合游戏引擎）
    mi_option_set(mi_option_reserve_huge_os_pages, 0); // 关闭大页面（小游戏不需要）
    
    // 3. 调试模式下的性能调优
    #ifdef _DEBUG
    mi_option_set(mi_option_show_errors, 1);        // 显示错误
    mi_option_set(mi_option_show_stats, 0);         // 不显示统计（性能）
    #endif
    
    // 4. 多线程优化
    // mimalloc默认配置已经很好了，通常不需要调整
    
    // 5. 针对特定内存模式优化（可选）
    // mi_option_set(mi_option_large_os_pages, 1);   // 大内存应用可启用
}
#endif