// ====================================================================
// 模块：LockFreeQueue
// 描述：基于 Michael-Scott 算法的无锁队列实现
//       使用 CAS（比较并交换）原子操作，支持多线程并发入队/出队
// 依赖：C++17 标准库（<atomic>, <memory>, <optional>）
// 特点：
//   - 无锁，无阻塞，高并发友好
//   - 使用哨兵节点（dummy node）简化边界处理
//   - 数据由 std::shared_ptr 管理，自动释放
//   - 强异常安全（操作失败时队列状态不变）
// ====================================================================

#include <atomic>
#include <memory>
#include <optional>

template<typename T>
class LockFreeQueue {
private:
    // ================================================================
    // 内部节点结构
    // ================================================================
    struct Node {
        std::shared_ptr<T> data;        // 指向实际数据（使用 shared_ptr 管理生命周期）
        std::atomic<Node*> next;        // 指向下一个节点的原子指针

        // 构造函数：next 初始化为 nullptr，data 默认为空
        Node() : next(nullptr) {}
    };

    // ================================================================
    // 成员变量
    // ================================================================
    std::atomic<Node*> head_;           // 队列头指针（指向哨兵节点）
    std::atomic<Node*> tail_;           // 队列尾指针（指向最后一个节点）

public:
    // ================================================================
    // 构造 / 析构
    // ================================================================

    // 创建一个空队列：初始化一个哨兵节点，头尾都指向它
    LockFreeQueue() {
        Node* dummy = new Node();       // 哨兵节点，不存储数据
        head_.store(dummy);
        tail_.store(dummy);
    }

    // 析构函数：释放所有节点（包括哨兵）
    ~LockFreeQueue() {
        while (Node* old_head = head_.load()) {
            head_.store(old_head->next);  // 头指针后移
            delete old_head;              // 删除原头节点
        }
    }

    // 禁止拷贝（无锁队列不支持拷贝）
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    // ================================================================
    // 入队操作（enqueue）
    // ================================================================

    void enqueue(T value) {
        // 1. 创建新节点，并将数据存入 shared_ptr
        std::shared_ptr<T> new_data = std::make_shared<T>(std::move(value));
        Node* new_node = new Node();
        new_node->data = new_data;        // 新节点 data 指向实际数据

        Node* tail;                       // 当前 tail 指针的本地副本
        while (true) {
            // 2. 原子读取当前尾指针（acquire 确保看到之前的写入）
            tail = tail_.load(std::memory_order_acquire);
            // 3. 读取 tail 的 next 指针（同样用 acquire）
            Node* next = tail->next.load(std::memory_order_acquire);

            // 4. 重新检查 tail 是否仍然是最新的（防止 ABA 和并发修改）
            if (tail == tail_.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    // 5. tail 确实是最后一个节点，尝试链接新节点
                    //    使用 CAS（compare_exchange_weak）将 tail->next 从 nullptr 改为 new_node
                    //    memory_order_release 确保新节点对后续线程可见
                    if (tail->next.compare_exchange_weak(
                            next, new_node,
                            std::memory_order_release,
                            std::memory_order_relaxed)) {
                        // 6. 链接成功，退出循环
                        break;
                    }
                    // CAS 失败（其他线程已经修改了 tail->next），重试循环
                } else {
                    // 7. tail 落后了（next 非空），说明其他线程已插入新节点但未更新 tail
                    //    我们帮助推进 tail 指针（CAS 尝试将 tail_ 更新为 next）
                    tail_.compare_exchange_weak(
                        tail, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                    // 无论 CAS 是否成功，都会继续循环，重新读取 tail
                }
            }
            // 如果 tail 与 tail_.load() 不一致，说明其他线程修改了 tail，重新循环
        }

        // 8. 尝试推进 tail 指针指向新节点（即使失败也没关系，其他线程会帮忙）
        //    这里使用 compare_exchange_weak，如果 tail 仍指向旧节点，则更新为 new_node
        tail_.compare_exchange_weak(
            tail, new_node,
            std::memory_order_release,
            std::memory_order_relaxed);
    }

    // ================================================================
    // 出队操作（dequeue）
    // ================================================================

    std::optional<T> dequeue() {
        Node* head;
        while (true) {
            // 1. 原子读取当前头指针（acquire）
            head = head_.load(std::memory_order_acquire);
            // 2. 读取当前尾指针（用于判断队列是否为空或 tail 落后）
            Node* tail = tail_.load(std::memory_order_acquire);
            // 3. 读取头节点的 next（即第一个实际节点）
            Node* next = head->next.load(std::memory_order_acquire);

            // 4. 检查 head 是否仍然是最新的（防止 ABA）
            if (head == head_.load(std::memory_order_acquire)) {
                if (head == tail) {
                    // 5. 头尾指向同一个节点（要么空队列，要么 tail 落后）
                    if (next == nullptr) {
                        // 队列为空（哨兵节点无后继）
                        return std::nullopt;
                    }
                    // tail 落后，帮助推进 tail 到 next
                    tail_.compare_exchange_weak(
                        tail, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                    // 继续循环，重新读取状态
                } else {
                    // 6. head 和 tail 不同，说明有至少一个实际节点
                    if (next == nullptr) {
                        // 理论上不可能，但为防止竞争，重试
                        continue;
                    }
                    // 7. 取出 next 节点中的数据（拷贝 shared_ptr）
                    std::shared_ptr<T> res = next->data;

                    // 8. 尝试推进 head 指针：将 head_ 从 head 更新为 next
                    //    使用 release 确保对后续线程可见
                    if (head_.compare_exchange_weak(
                            head, next,
                            std::memory_order_release,
                            std::memory_order_relaxed)) {
                        // 9. 成功推进 head，现在可以安全删除旧的哨兵节点
                        //    注意：此时 head 指针已经指向 next（实际节点），旧哨兵已被移除
                        T result = *res;    // 拷贝数据（若 T 较大可考虑移动）
                        delete head;        // head 是旧哨兵节点的指针（注意：此处 head 是局部变量，在 CAS 成功后仍指向旧哨兵）
                        return result;
                    }
                    // CAS 失败，重试循环
                }
            }
        }
    }

    // ================================================================
    // 辅助函数（非原子快照，仅用于调试）
    // ================================================================

    // 判断队列是否为空（注意：返回值不一定是原子准确的，只作参考）
    bool empty() const {
        Node* head = head_.load(std::memory_order_acquire);
        Node* tail = tail_.load(std::memory_order_acquire);
        return (head == tail) && (head->next.load() == nullptr);
    }
};
