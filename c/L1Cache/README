# Cache 模拟器 — L1 组相联 Cache 实现

## 项目简介

这是一个用 C 语言实现的 **L1 Cache 行为模拟器**，模拟了典型 CPU 缓存的工作机制，包括：

- 组相联映射（Set-Associative Mapping）
- 写回策略（Write-Back）+ 写分配（Write-Allocate）
- LRU 替换算法（Least Recently Used）
- 地址拆解（Tag / Index / Offset）
- 命中率、缺失率、写回次数统计

通过模拟 Cache 的读写行为，可以直观理解缓存对程序性能的影响。

---

## 核心特性

- **可配置的 Cache 参数**：总容量（默认 32KB）、路数（8-way）、缓存行大小（64B）
- **完整的地址拆解**：自动解析 tag、index、offset
- **LRU 替换策略**：使用全局时钟计数器追踪最近使用情况
- **写回策略**：脏行被替换时才写回主存，减少内存带宽占用
- **读写接口**：支持单字节和批量读写（`cache_read` / `cache_write`）
- **统计信息**：输出命中率、缺失率、写回次数

---

## 编译与运行

```bash
# 编译
gcc -o cache_sim cache_sim.c -std=c99 -Wall

# 运行
./cache_sim
```

---

## 配置参数（顶部宏定义）

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `CACHE_SIZE` | 32 * 1024 | 总容量（字节） |
| `LINE_SIZE` | 64 | 每个缓存行大小（字节） |
| `WAYS` | 8 | 每组路数（组相联度） |
| `OFFSET_BITS` | 6 | `log2(LINE_SIZE)` |
| `INDEX_BITS` | 6 | `log2(NUM_SETS)` |
| `MEM_SIZE` | 16 * 1024 * 1024 | 模拟主存大小（字节） |

这些参数共同决定：
- 总行数：`NUM_LINES = CACHE_SIZE / LINE_SIZE`
- 组数：`NUM_SETS = NUM_LINES / WAYS`

---

## 数据结构

```c
// 单个 Cache Line
typedef struct {
    bool     valid;          // 有效位
    bool     dirty;          // 脏位（写回策略）
    uint64_t tag;            // 标记位
    uint32_t lru_counter;    // LRU计数器
    uint8_t  data[LINE_SIZE]; // 实际缓存数据
} CacheLine;

// 一组（包含 WAYS 条 Cache Line）
typedef struct {
    CacheLine lines[WAYS];
} CacheSet;

// L1 Cache
typedef struct {
    CacheSet sets[NUM_SETS];
    uint32_t global_clock;
    uint64_t hits, misses, writebacks;
} L1Cache;
```

---

## API 参考

| 函数 | 说明 |
|------|------|
| `void cache_init(L1Cache *cache)` | 初始化 Cache（清空所有状态） |
| `uint8_t cache_read(L1Cache *cache, uint64_t addr)` | 从指定地址读取一个字节 |
| `void cache_write(L1Cache *cache, uint64_t addr, uint8_t value)` | 向指定地址写入一个字节 |
| `void cache_read_block(L1Cache *cache, uint64_t addr, uint8_t *buf, size_t len)` | 批量读取 |
| `void cache_write_block(L1Cache *cache, uint64_t addr, const uint8_t *buf, size_t len)` | 批量写入 |
| `void cache_print_stats(L1Cache *cache)` | 打印命中率、缺失率等统计信息 |

---

## 内部机制详解

### 地址拆解

对于给定的 64 位地址，拆分为三部分：

```
[  Tag bits  |  Index bits  |  Offset bits  ]
```

- **Offset**：定位缓存行内的字节（`LINE_SIZE` 决定）
- **Index**：选择组号（`NUM_SETS` 决定）
- **Tag**：与缓存行中的标记比较，判断是否命中

### 读操作流程

1. 解析地址 → 获得 tag、index、offset
2. 在 `sets[index]` 中查找 tag 匹配且 valid 的行
3. 若命中 → 更新 LRU 计数器，返回 `data[offset]`
4. 若未命中 → 选择 LRU 行（优先淘汰无效行），写回脏数据（若需要），从主存加载新行，返回数据

### 写操作流程

1. 解析地址 → 获得 tag、index、offset
2. 在 `sets[index]` 中查找 tag 匹配且 valid 的行
3. 若命中 → 更新 LRU 计数器，写入数据，标记 dirty
4. 若未命中 → 写分配（Write Allocate）：先加载该行，再写入数据，标记 dirty

### LRU 替换策略

- 每次命中或加载新行时，将该行的 `lru_counter` 设为 `++global_clock`
- 替换时，选择 `lru_counter` **最小** 的行（表示最久未使用）
- 无效行（`valid == false`）优先被淘汰

---

## 测试结果解读

程序运行后包含 4 个测试：

### 测试 1：顺序访问（空间局部性）
- 访问 1024 个连续字节
- **预期**：首次访问 miss，后续在同一缓存行内的访问命中，命中率约 85%-95%

### 测试 2：重复访问同一区域（时间局部性）
- 5 轮访问 256 字节
- **预期**：首轮 miss 后，后续轮次完全命中，命中率接近 100%

### 测试 3：写操作 + 脏行回写
- 写入 512 字节，然后访问新地址区域触发替换
- **预期**：脏行被替换时触发 writeback，统计可见写回次数

### 测试 4：跨步访问（Cache 不友好）
- 步长 = 2 × LINE_SIZE，每次访问不同缓存行
- **预期**：命中率显著降低（几乎每次访问都是 miss）

---

## 注意事项

- 模拟主存大小 `MEM_SIZE` 为 16MB，超过此范围的访问会打印错误并跳过。
- 统计信息为累计值，不自动清零。如需重新测试，请重新初始化 Cache。
- 批量读写接口（`cache_read_block` / `cache_write_block`）通过循环调用单字节接口实现，适合教学演示，非高性能设计。

---

## 扩展方向

- 支持多级缓存（L1 / L2 / L3）模拟
- 加入预取（Prefetch）策略
- 统计不同替换算法的命中率对比（FIFO / Random / PLRU）
- 模拟伪共享（False Sharing）现象
- 读入真实程序的内存访问 trace 文件

---

## 版本信息
- 当前版本：1.0

- 最后更新：2026-08-31

- 作者：根据用户需求生成
