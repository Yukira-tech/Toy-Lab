#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/*
 * =============================================================
 * 内存池 (Memory Pool) 实现
 * =============================================================
 *
 * 目的：减少频繁 malloc/free 带来的性能损耗和内存碎片。
 * 原理：一次性向系统申请一大块内存，切成固定大小的小块，
 *       由池子自行管理这些块的分配与回收。
 *
 * 适用场景：频繁创建/销毁相同大小对象的程序（如游戏实体、
 *          网络消息包、数据库连接等）。
 *
 * 核心结构：
 *   - MemBlock：每个小块的头部（仅包含一个 next 指针），
 *               用于串联空闲块。
 *   - MemChunk：每次从系统 malloc 的一大块内存的元数据，
 *               用于统一释放。
 *   - MemPool：管理整个池子的状态。
 *
 * 空闲链表机制：
 *   所有空闲块通过 next 指针串成单向链表，分配时取链表头，
 *   归还时插回链表头。
 * =============================================================
 */


/* =============================================================
 * 1. 数据结构定义
 * ============================================================= */

/**
 * struct MemBlock - 内存块的头部（与用户数据共用内存）
 *
 * 每个空闲块的前 sizeof(MemBlock) 字节被用来存储 next 指针，
 * 当块被分配出去后，这部分内存完全归用户使用。
 *
 * 因此，block_size 必须 >= sizeof(MemBlock)，否则无法串成链表。
 */
typedef struct MemBlock {
    struct MemBlock *next;  /* 指向下一个空闲块，NULL 表示链表末尾 */
} MemBlock;

/**
 * struct MemChunk - 内存块组（Chunk）的元数据
 *
 * 每个 Chunk 是一次 malloc 大内存的产物，里面包含多个 block。
 * 使用链表串联所有 Chunk，便于在销毁池子时统一释放。
 */
typedef struct MemChunk {
    struct MemChunk *next;  /* 指向下一个 Chunk（用于整体释放） */
    unsigned char *data;    /* 指向实际内存区域的起始地址 */
} MemChunk;

/**
 * struct MemPool - 内存池主结构
 *
 * 所有状态集中在此，对外接口的操作都围绕它展开。
 */
typedef struct MemPool {
    size_t block_size;       /* 每个块的实际大小（已对齐，>= sizeof(MemBlock)） */
    size_t blocks_per_chunk; /* 每次扩容时一个 Chunk 包含多少个块 */
    MemBlock *free_list;     /* 空闲块链表头指针（所有空闲块通过 next 串联） */
    MemChunk *chunk_list;    /* 已分配的 Chunk 链表头（用于析构） */
    size_t total_blocks;     /* 池子中块的总数（统计用） */
    size_t used_blocks;      /* 当前已分配的块数（统计用） */
} MemPool;


/* =============================================================
 * 2. 内部工具函数（static，对外不可见）
 * ============================================================= */

/**
 * align_size - 将 size 向上对齐到指针大小（sizeof(void *)）
 *
 * 对齐原因：
 *   1. 内存对齐可以提高 CPU 访问效率。
 *   2. 保证指针（MemBlock *）能够正确对齐。
 *
 * 算法： (size + align - 1) & ~(align - 1)
 *   例如：align=8，size=5 → (5+7)&~7 = 12&~7 = 8
 *
 * @param size  原始大小
 * @return      对齐后的大小
 */
static size_t align_size(size_t size) {
    const size_t align = sizeof(void *);
    return (size + align - 1) & ~(align - 1);
}

/**
 * pool_grow - 为内存池扩容（新增一个 Chunk）
 *
 * 执行步骤：
 *   1. 分配一个 MemChunk 元数据结构。
 *   2. 分配一大块连续内存（block_size * blocks_per_chunk 字节）。
 *   3. 将新 Chunk 挂到 chunk_list 头部。
 *   4. 将新内存按 block_size 切割，逐个串到 free_list 头部。
 *
 * @param pool  要扩容的内存池
 * @return      成功返回 true，失败返回 false（系统内存不足）
 */
static bool pool_grow(MemPool *pool) {
    /* 计算本次需要分配的总字节数 */
    size_t alloc_size = pool->block_size * pool->blocks_per_chunk;

    /* 1. 分配 Chunk 元数据 */
    MemChunk *chunk = (MemChunk *)malloc(sizeof(MemChunk));
    if (!chunk) return false;

    /* 2. 分配实际内存 */
    chunk->data = (unsigned char *)malloc(alloc_size);
    if (!chunk->data) {
        free(chunk);
        return false;
    }

    /* 3. 将新 Chunk 挂入链表头部（方便统一释放） */
    chunk->next = pool->chunk_list;
    pool->chunk_list = chunk;

    /* 4. 将新内存切割成块，串入空闲链表 */
    for (size_t i = 0; i < pool->blocks_per_chunk; i++) {
        /* 计算第 i 个块的起始地址 */
        MemBlock *block = (MemBlock *)(chunk->data + i * pool->block_size);
        /* 头插法：新块指向原链表头，然后更新链表头为新块 */
        block->next = pool->free_list;
        pool->free_list = block;
    }

    pool->total_blocks += pool->blocks_per_chunk;
    return true;
}


/* =============================================================
 * 3. 对外公开接口（API）
 * ============================================================= */

/**
 * mempool_create - 创建并初始化一个内存池
 *
 * @param block_size         每个对象的大小（推荐直接传 sizeof(YourType)）
 * @param blocks_per_chunk   每次扩容时分配多少个块（建议 16~64）
 *
 * @return                   返回 MemPool 指针，失败返回 NULL
 *
 * 注意事项：
 *   - block_size 会被自动对齐到指针大小，并保证至少能存放一个指针。
 *   - 创建时会自动进行第一次扩容（预分配 blocks_per_chunk 个块）。
 */
MemPool *mempool_create(size_t block_size, size_t blocks_per_chunk) {
    /* 参数合法性校验 */
    if (block_size == 0 || blocks_per_chunk == 0) return NULL;

    /* 分配池子主结构 */
    MemPool *pool = (MemPool *)malloc(sizeof(MemPool));
    if (!pool) return NULL;

    /*
     * 确保每个块至少能容纳一个 MemBlock（即一个指针），
     * 因为空闲块需要存储 next 指针。
     */
    size_t real_size = align_size(block_size);
    if (real_size < sizeof(MemBlock)) {
        real_size = sizeof(MemBlock);
    }

    /* 填充结构体字段 */
    pool->block_size = real_size;
    pool->blocks_per_chunk = blocks_per_chunk;
    pool->free_list = NULL;
    pool->chunk_list = NULL;
    pool->total_blocks = 0;
    pool->used_blocks = 0;

    /* 首次扩容（预分配内存） */
    if (!pool_grow(pool)) {
        free(pool);
        return NULL;
    }

    return pool;
}

/**
 * mempool_alloc - 从内存池中分配一个块
 *
 * 算法：
 *   1. 如果空闲链表为空，调用 pool_grow() 扩容。
 *   2. 从空闲链表中取出第一个块，返回给用户。
 *
 * @param pool  目标内存池
 * @return      返回用户数据区指针（与分配地址相同），失败返回 NULL
 *
 * 时间复杂度：O(1)
 */
void *mempool_alloc(MemPool *pool) {
    if (!pool) return NULL;

    /* 空闲链表为空 → 需要扩容 */
    if (!pool->free_list) {
        if (!pool_grow(pool)) {
            return NULL;  /* 系统内存耗尽 */
        }
    }

    /* 从链表头取下一个空闲块 */
    MemBlock *block = pool->free_list;
    pool->free_list = block->next;  /* 链表头后移 */
    pool->used_blocks++;

    /* 返回块起始地址（用户无需关心 MemBlock 头部） */
    return (void *)block;
}

/**
 * mempool_free - 将块归还给内存池
 *
 * 注意：此操作不会将内存归还给操作系统，只是重新插入空闲链表。
 *       因此被释放的内存可以被后续的 mempool_alloc 再次分配。
 *
 * @param pool  目标内存池
 * @param ptr   要归还的块地址（必须是由 mempool_alloc 返回的指针）
 *
 * 时间复杂度：O(1)
 */
void mempool_free(MemPool *pool, void *ptr) {
    if (!pool || !ptr) return;

    MemBlock *block = (MemBlock *)ptr;
    block->next = pool->free_list;  /* 头插法插入空闲链表 */
    pool->free_list = block;
    pool->used_blocks--;
}

/**
 * mempool_destroy - 销毁整个内存池，释放所有底层内存
 *
 * 执行步骤：
 *   1. 遍历 chunk_list，释放每个 Chunk 的 data 和元数据。
 *   2. 释放 MemPool 本身。
 *
 * 注意：销毁前应确保所有已分配的块不再被使用，否则会产生悬空指针。
 *
 * @param pool  要销毁的内存池
 */
void mempool_destroy(MemPool *pool) {
    if (!pool) return;

    /* 遍历所有 Chunk，逐个释放 */
    MemChunk *chunk = pool->chunk_list;
    while (chunk) {
        MemChunk *next = chunk->next;
        free(chunk->data);  /* 释放实际内存 */
        free(chunk);        /* 释放元数据 */
        chunk = next;
    }

    free(pool);  /* 释放池子本身 */
}

/**
 * mempool_stats - 打印内存池的统计信息（调试用）
 *
 * @param pool  目标内存池
 */
void mempool_stats(MemPool *pool) {
    if (!pool) return;
    printf("Pool Stats: block_size=%zu, total=%zu, used=%zu, free=%zu\n",
           pool->block_size,
           pool->total_blocks,
           pool->used_blocks,
           pool->total_blocks - pool->used_blocks);
}


/* =============================================================
 * 4. 使用示例
 * ============================================================= */

/**
 * 示例数据类型：一个包含 id、name、value 的结构体
 */
typedef struct {
    int id;
    char name[32];
    double value;
} MyObject;

int main(void) {
    /*
     * 创建内存池：
     *   - 每个块大小 = sizeof(MyObject)
     *   - 每次扩容分配 16 个块
     */
    MemPool *pool = mempool_create(sizeof(MyObject), 16);
    if (!pool) {
        fprintf(stderr, "内存池创建失败\n");
        return 1;
    }

    mempool_stats(pool);  /* 初始状态：16 个空闲块 */

    /* 分配 20 个对象（会触发第二次扩容） */
    MyObject *objects[20];
    for (int i = 0; i < 20; i++) {
        objects[i] = (MyObject *)mempool_alloc(pool);
        if (objects[i]) {
            objects[i]->id = i;
            snprintf(objects[i]->name, sizeof(objects[i]->name), "obj_%d", i);
            objects[i]->value = i * 1.5;
        }
    }

    printf("分配 20 个对象后:\n");
    mempool_stats(pool);  /* 总块数应为 32（两次扩容） */

    /* 归还前 10 个对象（仅归还，不销毁） */
    for (int i = 0; i < 10; i++) {
        mempool_free(pool, objects[i]);
    }

    printf("归还 10 个对象后:\n");
    mempool_stats(pool);  /* used=10, free=22 */

    /* 验证未被归还的对象数据依然正确 */
    printf("验证: obj[15] -> id=%d, name=%s, value=%.2f\n",
           objects[15]->id, objects[15]->name, objects[15]->value);

    /* 销毁内存池，释放所有资源 */
    mempool_destroy(pool);

    return 0;
}
