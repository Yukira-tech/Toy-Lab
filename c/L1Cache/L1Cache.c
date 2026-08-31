#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// ============ 配置参数 ============
// 典型 L1 Cache: 32KB, 8-way, 64B cache line

#define CACHE_SIZE      (32 * 1024)   // 32KB 总容量
#define LINE_SIZE       64            // 每个 cache line 64字节
#define WAYS            8             // 8路组相联

#define NUM_LINES       (CACHE_SIZE / LINE_SIZE)      // 总行数 = 512
#define NUM_SETS        (NUM_LINES / WAYS)             // 组数 = 64

#define OFFSET_BITS     6   // log2(LINE_SIZE) = 6
#define INDEX_BITS      6   // log2(NUM_SETS)  = 6
// TAG_BITS = 地址位数 - OFFSET_BITS - INDEX_BITS

// ============ Cache Line 结构 ============

typedef struct {
    bool     valid;         // 有效位
    bool     dirty;         // 脏位（写回策略用）
    uint64_t tag;           // 标记位
    uint32_t lru_counter;   // LRU计数器（数值越小越久未使用）
    uint8_t  data[LINE_SIZE]; // 实际缓存数据
} CacheLine;

typedef struct {
    CacheLine lines[WAYS];  // 一组内的所有路
} CacheSet;

typedef struct {
    CacheSet sets[NUM_SETS];
    uint32_t global_clock;  // 用于LRU计数

    // 统计信息
    uint64_t hits;
    uint64_t misses;
    uint64_t writebacks;
} L1Cache;

// ============ 模拟主存 ============
// 简化：用一大块内存模拟主存，实际系统中这里是DRAM访问

#define MEM_SIZE (1024 * 1024 * 16)  // 16MB 模拟主存
static uint8_t main_memory[MEM_SIZE];

// ============ 地址拆解 ============

typedef struct {
    uint64_t tag;
    uint32_t index;
    uint32_t offset;
} AddrParts;

static AddrParts parse_addr(uint64_t addr) {
    AddrParts parts;
    parts.offset = addr & (LINE_SIZE - 1);
    parts.index  = (addr >> OFFSET_BITS) & (NUM_SETS - 1);
    parts.tag    = addr >> (OFFSET_BITS + INDEX_BITS);
    return parts;
}

// ============ Cache 初始化 ============

void cache_init(L1Cache *cache) {
    memset(cache, 0, sizeof(L1Cache));
    cache->global_clock = 0;
}

// ============ 从主存加载一行数据到cache line ============

static void load_line_from_memory(CacheLine *line, uint64_t line_base_addr) {
    if (line_base_addr + LINE_SIZE > MEM_SIZE) {
        fprintf(stderr, "地址越界: 0x%lx\n", line_base_addr);
        return;
    }
    memcpy(line->data, &main_memory[line_base_addr], LINE_SIZE);
}

// 写回脏数据到主存
static void writeback_line(L1Cache *cache, CacheLine *line, uint32_t set_idx) {
    if (line->valid && line->dirty) {
        uint64_t line_base_addr = ((line->tag << INDEX_BITS) | set_idx) << OFFSET_BITS;
        if (line_base_addr + LINE_SIZE <= MEM_SIZE) {
            memcpy(&main_memory[line_base_addr], line->data, LINE_SIZE);
        }
        cache->writebacks++;
    }
}

// ============ 查找命中的line，返回索引，-1表示未命中 ============

static int find_line(CacheSet *set, uint64_t tag) {
    for (int i = 0; i < WAYS; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            return i;
        }
    }
    return -1;
}

// ============ LRU替换：找到该组中最久未使用的line ============

static int find_lru_victim(CacheSet *set) {
    int victim = 0;
    uint32_t min_counter = set->lines[0].lru_counter;

    // 优先淘汰无效行
    for (int i = 0; i < WAYS; i++) {
        if (!set->lines[i].valid) {
            return i;
        }
        if (set->lines[i].lru_counter < min_counter) {
            min_counter = set->lines[i].lru_counter;
            victim = i;
        }
    }
    return victim;
}

// ============ 处理 Cache Miss：加载新行，可能触发替换 ============

static int handle_miss(L1Cache *cache, uint32_t set_idx, uint64_t tag) {
    CacheSet *set = &cache->sets[set_idx];
    int victim_idx = find_lru_victim(set);
    CacheLine *victim = &set->lines[victim_idx];

    // 若被替换的行是脏的，需要写回主存（写回策略）
    writeback_line(cache, victim, set_idx);

    // 加载新数据
    uint64_t line_base_addr = ((tag << INDEX_BITS) | set_idx) << OFFSET_BITS;
    load_line_from_memory(victim, line_base_addr);

    victim->valid = true;
    victim->dirty = false;
    victim->tag = tag;
    victim->lru_counter = ++cache->global_clock;

    return victim_idx;
}

// ============ 核心API: 读取一个字节 ============

uint8_t cache_read(L1Cache *cache, uint64_t addr) {
    AddrParts p = parse_addr(addr);
    CacheSet *set = &cache->sets[p.index];

    int line_idx = find_line(set, p.tag);

    if (line_idx >= 0) {
        // Cache Hit
        cache->hits++;
        set->lines[line_idx].lru_counter = ++cache->global_clock;
        return set->lines[line_idx].data[p.offset];
    } else {
        // Cache Miss
        cache->misses++;
        line_idx = handle_miss(cache, p.index, p.tag);
        return set->lines[line_idx].data[p.offset];
    }
}

// ============ 核心API: 写入一个字节（写回策略 write-back + write-allocate）============

void cache_write(L1Cache *cache, uint64_t addr, uint8_t value) {
    AddrParts p = parse_addr(addr);
    CacheSet *set = &cache->sets[p.index];

    int line_idx = find_line(set, p.tag);

    if (line_idx >= 0) {
        // Cache Hit
        cache->hits++;
        set->lines[line_idx].lru_counter = ++cache->global_clock;
    } else {
        // Cache Miss -> Write Allocate: 先加载该行
        cache->misses++;
        line_idx = handle_miss(cache, p.index, p.tag);
    }

    set->lines[line_idx].data[p.offset] = value;
    set->lines[line_idx].dirty = true;  // 标记为脏，稍后写回
}

// ============ 批量读写接口（模拟真实访问模式）============

void cache_read_block(L1Cache *cache, uint64_t addr, uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        buf[i] = cache_read(cache, addr + i);
    }
}

void cache_write_block(L1Cache *cache, uint64_t addr, const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        cache_write(cache, addr + i, buf[i]);
    }
}

// ============ 统计信息打印 ============

void cache_print_stats(L1Cache *cache) {
    uint64_t total = cache->hits + cache->misses;
    double hit_rate = total > 0 ? (100.0 * cache->hits / total) : 0.0;

    printf("===== L1 Cache 统计 =====\n");
    printf("配置: %dKB, %d-way, %dB line, %d sets\n",
           CACHE_SIZE / 1024, WAYS, LINE_SIZE, NUM_SETS);
    printf("总访问: %lu\n", total);
    printf("命中: %lu (%.2f%%)\n", cache->hits, hit_rate);
    printf("未命中: %lu (%.2f%%)\n", cache->misses, 100.0 - hit_rate);
    printf("写回次数: %lu\n", cache->writebacks);
    printf("=========================\n");
}

// ============ 测试程序 ============

int main(void) {
    L1Cache cache;
    cache_init(&cache);

    // 初始化模拟主存数据
    for (size_t i = 0; i < 1024; i++) {
        main_memory[i] = (uint8_t)(i & 0xFF);
    }

    printf("测试1: 顺序访问（体现空间局部性）\n");
    for (int i = 0; i < 1024; i++) {
        uint8_t val = cache_read(&cache, i);
        (void)val;
    }
    cache_print_stats(&cache);

    printf("\n测试2: 重复访问同一区域（体现时间局部性）\n");
    for (int round = 0; round < 5; round++) {
        for (int i = 0; i < 256; i++) {
            cache_read(&cache, i);
        }
    }
    cache_print_stats(&cache);

    printf("\n测试3: 写操作与脏行回写\n");
    for (int i = 0; i < 512; i++) {
        cache_write(&cache, i, (uint8_t)(i * 2));
    }
    // 触发大量替换，强制写回之前的脏数据
    for (int i = 2048; i < 4096; i++) {
        cache_read(&cache, i);
    }
    cache_print_stats(&cache);

    // 验证写回后数据一致性
    printf("\n验证地址100的数据: cache=%d, mem=%d\n",
           cache_read(&cache, 100), main_memory[100]);

    printf("\n测试4: 跨步访问（体现Cache不友好模式，命中率会降低）\n");
    L1Cache cache2;
    cache_init(&cache2);
    int stride = LINE_SIZE * 2; // 跨步超过line大小，几乎每次都miss
    for (int i = 0; i < 100; i++) {
        cache_read(&cache2, (i * stride) % (MEM_SIZE - LINE_SIZE));
    }
    cache_print_stats(&cache2);

    return 0;
}
