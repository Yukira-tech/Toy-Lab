# VECTOR_SPEC.md — myStd::STL::vector 接口说明

## 1. 概述

`myStd::STL::vector` 是一个**手搓的动态数组容器**，完全使用 C++20 模块语法实现，模仿 `std::vector` 的核心行为，但在接口和实现细节上做了若干调整，旨在提供**强异常安全保证**和**更直观的索引操作**。

本容器不属于任何官方标准库，而是一个**教学/实验性实现**，代码注释详尽，便于理解动态数组的底层原理。

---

## 2. 模块声明

```cpp
export module myStd.vector;
```

导入方式：
```cpp
import myStd.vector;
```

---

## 3. 模板参数

```cpp
template<typename T, typename Allocator = std::allocator<T>>
class vector;
```

- **`T`**：元素类型，必须为完整类型。
- **`Allocator`**：内存分配器类型，默认使用 `std::allocator<T>`，需满足分配器要求。

---

## 4. 类型别名

| 别名 | 定义 |
|------|------|
| `value_type` | `T` |
| `allocator_type` | `Allocator` |
| `iterator` | `T*` |
| `const_iterator` | `const T*` |

---

## 5. 构造与析构

| 构造函数 | 说明 |
|----------|------|
| `vector()` | 默认构造，空容器。 |
| `explicit vector(std::size_t n)` | 构造 `n` 个默认初始化的元素（`T` 需可默认构造）。 |
| `vector(std::size_t n, const value_type& val)` | 构造 `n` 个值为 `val` 的元素。 |
| `vector(std::initializer_list<value_type> list)` | 使用列表初始化。 |
| `vector(const vector& rhs)` | 拷贝构造（深拷贝）。 |
| `vector(vector&& rhs) noexcept` | 移动构造（偷取资源）。 |
| `~vector()` | 析构，销毁所有元素并释放内存。 |

---

## 6. 赋值与交换

| 成员 | 说明 |
|------|------|
| `vector& operator=(const vector& rhs)` | 拷贝赋值（使用 copy-swap 惯用法，强异常安全）。 |
| `vector& operator=(vector&& rhs) noexcept` | 移动赋值（偷取资源）。 |
| `void swap(vector& other) noexcept` | 交换两个容器的内容（成员 swap）。 |

---

## 7. 迭代器

所有迭代器均为原生指针，满足随机访问迭代器要求。

| 成员 | 说明 |
|------|------|
| `iterator begin() noexcept` | 返回指向首元素的迭代器。 |
| `const_iterator begin() const noexcept` | 常量版本。 |
| `const_iterator cbegin() const noexcept` | 常量迭代器。 |
| `iterator end() noexcept` | 返回指向末尾后一位置的迭代器。 |
| `const_iterator end() const noexcept` | 常量版本。 |
| `const_iterator cend() const noexcept` | 常量迭代器。 |

---

## 8. 容量

| 成员 | 说明 |
|------|------|
| `std::size_t size() const noexcept` | 返回当前元素数。 |
| `std::size_t capacity() const noexcept` | 返回当前容量（已分配内存可容纳的元素数）。 |
| `bool empty() const noexcept` | 判断是否为空。 |
| `void reserve(std::size_t new_capacity)` | 预留容量，若 `new_capacity > capacity()` 则重新分配，否则无操作。 |
| `void resize(std::size_t new_size, value_type val = value_type{})` | 改变元素数，若新大小大于当前，则追加 `val` 的副本；若小于，则删除多余元素。 |
| `void shrink_to_fit()` | 将容量收缩到当前大小（若 `size() < capacity()` 则重新分配，否则无操作）。 |

---

## 9. 元素访问

| 成员 | 说明 |
|------|------|
| `value_type& operator[](std::size_t idx)` | 下标访问，**执行边界检查**，越界抛出 `std::out_of_range`。 |
| `const value_type& operator[](std::size_t idx) const` | 常量版本，同样检查边界。 |
| `value_type& at(std::size_t idx)` | 与 `operator[]` 行为相同（也检查边界）。 |
| `const value_type& at(std::size_t idx) const` | 常量版本。 |
| `value_type& front()` | 返回首元素引用，空时抛出 `std::out_of_range`。 |
| `const value_type& front() const` | 常量版本。 |
| `value_type& back()` | 返回末尾元素引用，空时抛出 `std::out_of_range`。 |
| `const value_type& back() const` | 常量版本。 |
| `value_type* data() const noexcept` | 返回底层数组指针。 |

---

## 10. 修改器（单元素）

| 成员 | 说明 |
|------|------|
| `void push_back(const value_type& val)` | 在末尾插入 `val` 的副本（若容量不足则扩容）。 |
| `void pop_back()` | 删除末尾元素，空时抛出 `std::out_of_range`。 |
| `void insert(std::size_t idx, value_type val)` | 在索引 `idx` 处插入元素（原有元素后移），`idx` 须 ≤ `size()`，否则抛出 `std::out_of_range`。 |
| `void erase(std::size_t idx)` | 删除索引 `idx` 处的元素，`idx` 须 < `size()`，否则抛出 `std::out_of_range`。 |
| `void clear() noexcept` | 删除所有元素，容量不变（内存不释放）。 |
| `void assign(std::initializer_list<value_type> list)` | 用列表内容替换当前所有元素（若列表大小大于容量则扩容）。 |
| `void emplace(std::size_t idx, value_type val)` | 在 `idx` 处直接构造元素（等价于 `insert(idx, val)`）。 |
| `void emplace_back(value_type val)` | 在末尾直接构造元素（等价于 `push_back(val)`）。 |

---

## 11. 范围操作（扩展接口）

这些函数不是 `std::vector` 的标准接口，而是本实现提供的便利方法。

| 成员 | 说明 |
|------|------|
| `void append_range(std::initializer_list<value_type> list)` | 追加列表中的所有元素到末尾。 |
| `void insert_range(std::size_t idx, std::initializer_list<value_type> list)` | 在 `idx` 处插入列表中的所有元素，原有元素后移。 |
| `void assign_range(std::initializer_list<value_type> list)` | 等同于 `assign(list)`。 |
| `void erase_range(std::size_t idx, std::size_t count)` | 从 `idx` 开始连续删除 `count` 个元素，若范围越界则抛出 `std::out_of_range`。 |

---

## 12. 分配器

| 成员 | 说明 |
|------|------|
| `Allocator get_allocator() const noexcept` | 返回分配器副本。 |

---

## 13. 非成员比较运算符

所有比较运算符按**字典序**进行。

| 运算符 | 说明 |
|--------|------|
| `bool operator==(const vector& lhs, const vector& rhs)` | 判断两个容器是否相等（元素逐个比较）。 |
| `bool operator!=(const vector& lhs, const vector& rhs)` | 不相等。 |
| `bool operator<(const vector& lhs, const vector& rhs)` | 小于。 |
| `bool operator>(const vector& lhs, const vector& rhs)` | 大于。 |
| `bool operator<=(const vector& lhs, const vector& rhs)` | 小于等于。 |
| `bool operator>=(const vector& lhs, const vector& rhs)` | 大于等于。 |

---

## 14. 异常安全保证

- **所有操作**均提供**强异常安全保证**（commit-or-rollback）。
- 在 `reallocate` 过程中，若移动构造或拷贝构造失败，已分配的新内存会被自动释放，原对象状态保持不变。
- 析构函数 `noexcept`，绝不抛出异常。

---

## 15. 内存扩容策略

- 当 `size() == capacity()` 时触发扩容。
- 扩容因子固定为 **2**（即新容量 = 旧容量 × 2）。
- 空容器（`capacity() == 0`）首次插入时容量变为 **1**。
- 扩容操作调用 `reallocate(new_capacity)`，内部通过分配器分配新内存，并移动（或拷贝）原有元素到新内存。

---

## 16. 与 `std::vector` 的差异总结

| 差异点 | `std::vector` | `myStd::STL::vector` |
|--------|---------------|----------------------|
| **`operator[]` 边界检查** | 不检查（未定义行为） | **检查**，越界抛 `std::out_of_range` |
| **`insert` / `erase` 位置参数** | 迭代器 | **索引（`std::size_t`）** |
| **范围操作接口** | `insert(pos, first, last)` | `insert_range(idx, initializer_list)` |
| **批量删除** | `erase(first, last)` | `erase_range(idx, count)` |
| **迭代器类型** | 专用迭代器类（通常为指针包装） | **原生指针**（`T*`） |
| **模块支持** | 传统头文件 | C++20 模块 |
| **扩容因子** | 实现定义（通常 2 倍） | 固定 2 倍 |
| **异常安全** | 强异常安全（标准要求） | 同样强异常安全 |

---

## 17. 使用示例

```cpp
import myStd.vector;
#include <iostream>

int main() {
    myStd::STL::vector<int> v;
    v.push_back(10);
    v.push_back(20);
    v.push_back(30);

    v.insert(1, 15);          // 在索引1插入15
    v.erase_range(2, 2);      // 删除索引2开始的两个元素

    for (int x : v) {
        std::cout << x << " ";  // 输出: 10 15
    }

    v.resize(5, 100);         // 容量不足时自动扩容
    std::cout << v.size();    // 5
    std::cout << v[3];        // 100
}
```

---

## 18. 编译需求

- C++20 编译器（支持模块）。
- 标准库头文件：`<cstddef>`、`<memory>`、`<algorithm>`、`<stdexcept>`。

---

**版本**：1.0  
**最后更新**：2026-08-31  
**作者**：Yukira
