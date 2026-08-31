# LockFreeQueue — 基于 Michael-Scott 算法的无锁队列实现

## 项目简介

这是一个基于 C++ 原子操作实现的**无锁（Lock-Free）并发队列**，采用经典的 **Michael-Scott 算法**（M&S 队列）。它使用 `std::atomic` 和 `std::shared_ptr` 管理内存，实现了无锁的入队（enqueue）和出队（dequeue）操作，适用于高并发场景下线程安全的 FIFO 数据传递。

---

## 核心特性

- **无锁设计**：不使用互斥锁（mutex），所有操作基于 CAS（Compare-And-Swap）原子操作，避免上下文切换和死锁。
- **Michael-Scott 算法**：使用哨兵节点（dummy head）简化边界条件，通过原子操作同时维护头尾指针。
- **内存安全**：节点数据使用 `std::shared_ptr` 管理，自动处理生命周期，避免显式内存管理。
- **强异常安全**：入队和出队操作不会抛出异常（除内存分配失败外），且在失败时保持队列状态一致。
- **支持任意类型**：模板化设计，可存储任意可移动/拷贝的类型。

---

## API 参考

### 构造与析构

| 成员 | 说明 |
|------|------|
| `LockFreeQueue()` | 默认构造，创建一个空队列（内部创建哨兵节点）。 |
| `~LockFreeQueue()` | 析构，释放所有剩余节点（包括哨兵）。 |

### 核心操作

| 成员 | 说明 |
|------|------|
| `void enqueue(T value)` | 将 `value` 入队，使用移动语义。 |
| `std::optional<T> dequeue()` | 出队，若队列为空返回 `std::nullopt`；否则返回元素值。 |
| `bool empty() const` | 检查队列是否为空（**非原子快照**，仅用于调试）。 |

### 禁用拷贝

- 拷贝构造和拷贝赋值被删除，不支持复制队列。

---

## 实现细节

### 数据结构

```cpp
struct Node {
    std::shared_ptr<T> data;   // 实际数据（由 shared_ptr 管理）
    std::atomic<Node*> next;   // 指向下一个节点的原子指针
};
```

- 使用 **哨兵节点**（dummy node）：头指针指向一个不存储数据的节点，简化了空队列和边界条件的处理。
- `head_` 和 `tail_` 均为 `std::atomic<Node*>`，保证原子更新。

### 入队（enqueue）

1. 创建新节点，并封装数据到 `shared_ptr`。
2. 循环执行：
   - 读取当前 `tail` 和 `tail->next`。
   - 验证 `tail` 是否仍是最新的。
   - 如果 `tail->next == nullptr`，尝试用 CAS 将新节点链接到尾部。
   - 如果链接成功，跳出循环。
   - 否则帮助推进 `tail`（`tail` 可能已被其他线程更新）。
3. 尝试将 `tail` 更新为新节点（即使失败，其他线程会帮忙）。

### 出队（dequeue）

1. 循环执行：
   - 读取当前 `head`、`tail` 和 `head->next`。
   - 验证 `head` 是否仍是最新的。
   - 如果 `head == tail`：
     - 若 `next == nullptr`，队列为空，返回 `std::nullopt`。
     - 否则 `tail` 落后，帮助推进 `tail`。
   - 否则：
     - 读取 `next->data`（实际数据）。
     - 尝试用 CAS 推进 `head` 到 `next`。
     - 若成功，删除旧的哨兵节点，返回数据。

### 内存管理

- 使用 `std::shared_ptr<T>` 存储数据：当节点被删除时，数据自动释放。
- 哨兵节点在出队时被删除，入队时始终创建新节点。
- 析构函数遍历链表并释放所有节点。

---

## 与标准队列的差异

| 特性 | `std::queue`（非并发） | `std::deque` + 锁 | 本实现（Michael-Scott 算法） |
|------|------------------------|-------------------|------------------------------|
| **线程安全** | 无（需外部锁） | 通过互斥锁实现 | **无锁**，完全并发安全 |
| **阻塞行为** | 无 | 锁会阻塞 | **非阻塞**（无等待，但入队可能重试） |
| **性能** | 单线程最快 | 锁竞争影响性能 | 高并发下优于加锁实现 |
| **内存管理** | 自动（容器内部） | 自动 | 手动管理节点，但使用 `shared_ptr` |
| **异常安全** | 强保证（容器标准） | 取决于锁实现 | **强保证**（操作中状态一致） |
| **`empty()` 检查** | 支持，且准确 | 支持，需加锁 | 仅提供“快照”，非原子（不保证准确） |
| **适用场景** | 单线程或低并发 | 低并发 | 高并发、对延迟敏感的系统 |

---

## 使用示例

```cpp
#include "LockFreeQueue.h"
#include <thread>
#include <iostream>

int main() {
    LockFreeQueue<int> q;

    // 生产者
    std::thread producer([&]() {
        for (int i = 0; i < 10; ++i) {
            q.enqueue(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // 消费者
    std::thread consumer([&]() {
        int count = 0;
        while (count < 10) {
            auto val = q.dequeue();
            if (val.has_value()) {
                std::cout << "Dequeued: " << *val << std::endl;
                ++count;
            }
        }
    });

    producer.join();
    consumer.join();
    return 0;
}
```

---

## 编译与依赖

- **C++ 标准**：C++17 或更高（需要 `std::optional` 和 `std::shared_ptr`）。
- **头文件**：`<atomic>`、`<memory>`、`<optional>`。
- **无需外部库**，仅依赖标准库。

编译命令示例：
```bash
g++ -std=c++17 -pthread main.cpp -o lockfree_queue
```
---

## 版本信息
- 当前版本：1.0

- 最后更新：2026-08-31

- 作者：根据用户需求生成
