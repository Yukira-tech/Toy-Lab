# 内存池（Memory Pool）— C 语言实现

## 项目简介

这是一个轻量级、高性能的 C 语言内存池实现。它通过一次性申请大块内存，然后自行切割和回收固定大小的对象，避免了频繁调用 `malloc` / `free` 的开销，减少了内存碎片。

---

## 核心特性

- 固定块大小，分配和回收均为 O(1) 时间复杂度。
- 自动扩容：空闲块耗尽时自动追加新 Chunk。
- 内存对齐：自动对齐到指针大小，保证访问效率。
- 无侵入性：用户数据直接存放在块中，无额外头部开销（空闲时才使用 next 指针）。
- 统一释放：`mempool_destroy` 一次释放所有内存，无内存泄漏。

---

## API 文档

### `MemPool *mempool_create(size_t block_size, size_t blocks_per_chunk)`

创建并初始化一个内存池。

- **block_size**：每个对象的大小（推荐直接传 `sizeof(YourType)`）。
- **blocks_per_chunk**：每次扩容时新增的块数量（建议 16~64）。
- **返回值**：成功返回 `MemPool*`，失败返回 `NULL`。

> 创建时会自动进行首次扩容。

---

### `void *mempool_alloc(MemPool *pool)`

从内存池中分配一个块。

- **pool**：目标内存池。
- **返回值**：返回用户数据区指针，失败返回 `NULL`。

> 当空闲链表为空时，会自动触发扩容。

---

### `void mempool_free(MemPool *pool, void *ptr)`

将已分配的块归还给内存池。

- **pool**：目标内存池。
- **ptr**：由 `mempool_alloc` 返回的指针。

> 归还后该块可被再次分配，但不归还给操作系统。

---

### `void mempool_destroy(MemPool *pool)`

销毁整个内存池，释放所有底层内存。

- **pool**：要销毁的内存池。

> 调用前请确保所有已分配的对象不再被使用，否则会产生悬空指针。

---

### `void mempool_stats(MemPool *pool)`

打印内存池的统计信息（仅用于调试）。

- **pool**：目标内存池。

输出格式：`block_size=xx, total=xx, used=xx, free=xx`

---

## 使用示例

```c
typedef struct {
    int id;
    char name[32];
    double value;
} MyObject;

int main(void) {
    MemPool *pool = mempool_create(sizeof(MyObject), 16);
    if (!pool) return 1;

    MyObject *obj = (MyObject *)mempool_alloc(pool);
    obj->id = 1;
    strcpy(obj->name, "test");

    mempool_free(pool, obj);
    mempool_destroy(pool);
    return 0;
}
完整示例见 main() 函数中的测试代码。
---
## 编译与运行
```bash
gcc -o mempool mempool.c -std=c99 -Wall
./mempool
```
---
## 适用场景
1. 游戏引擎中的实体对象池
2. 网络服务器的消息包缓冲池
3. 数据库连接池的内存管理

任何频繁创建/销毁同类型对象的场景
---
## 注意事项
块大小过小（小于指针大小）会被自动扩到指针大小。

所有分配的对象必须在销毁前归还，否则会造成内存泄漏（但池子销毁时会统一释放）。

本实现不考虑线程安全，多线程环境需自行加锁。

作者: Yukira
