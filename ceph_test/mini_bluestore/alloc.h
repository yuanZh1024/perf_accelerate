#ifndef ALLOC_H
#define ALLOC_H
#include <stdint.h>

typedef struct {
    uint8_t *bits;       // 位图，1 = 已用
    uint64_t total;      // 总块数
    uint64_t free_cnt;   // 空闲块数
} Allocator;

/**
 * 初始化位图分配器：按总块数分配 (total+7)/8 字节位图，全 0 表示全空闲。
 * @param a             输出参数，分配器结构体。
 * @param total_blocks  磁盘总块数（2GB / 4KB = 524288）。
 * @return              0 成功；-1 位图内存分配失败。
 */
int     alloc_init(Allocator *a, uint64_t total_blocks);
/**
 * 释放位图内存（不负责磁盘资源）。
 * @param a  已初始化的分配器。
 */
void    alloc_destroy(Allocator *a);
/**
 * first-fit 分配：从头扫描，找到第一段长度 >= count 的连续空闲块并标记已用。
 * @param a      分配器。
 * @param count  需要的连续块数。
 * @return       成功返回该段起始块号；空间不足返回 -1。
 */
int64_t alloc_alloc(Allocator *a, uint64_t count);               // 成功返回起始块号，失败返回 -1
/**
 * 释放 [start, start+count) 区间：对应位清零并增加 free_cnt（已空闲的位不动）。
 * @param a      分配器。
 * @param start  起始块号。
 * @param count  连续块数。
 */
void    alloc_free(Allocator *a, uint64_t start, uint64_t count);
/**
 * 标记 [start, start+count) 为已用（预留块、或从元数据重建位图时使用）。
 * @param a      分配器。
 * @param start  起始块号。
 * @param count  连续块数。
 * @note        教学版未做 start+count <= total 的边界检查。
 */
void    alloc_mark_used(Allocator *a, uint64_t start, uint64_t count);  // 预留/恢复时用
#endif
