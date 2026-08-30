// ============================================================================
// 模块：myStd.vector
// 描述：动态数组容器（顺序表）的模板实现
//       支持任意类型 T，提供随机访问、自动扩容、强异常安全保证
// 核心特性：
//   - 连续内存存储，缓存友好
//   - 容量倍增策略（扩容因子 2）
//   - 强异常安全（reallocate 失败时保持原对象不变）
//   - 支持移动语义，避免不必要的拷贝
//   - 迭代器为原生指针，零开销抽象
// 依赖：<cstddef>（size_t）、<memory>（allocator / uninitialized_*）、
//       <algorithm>（move / move_backward）、<stdexcept>（out_of_range）
// ============================================================================


export module myStd.vector;
import <cstddef>;
import <memory>;
import <algorithm>;
import <stdexcept>;

export namespace myStd::STL {
	// ========================================================================
	// 类模板：vector<value_type, Allocator>
	// 模板参数：
	//   value_type - 元素类型（可为任意完整类型）
	//   Allocator  - 内存分配器（默认为 std::allocator<value_type>）
	// 功能：动态数组，支持随机访问，自动扩容，强异常安全
	// 迭代器：原生指针（T*），满足随机访问迭代器要求
	// 复杂度：
	//   - 随机访问：O(1)
	//   - 末尾插入/删除：均摊 O(1)
	//   - 中间插入/删除：O(n)
	// ========================================================================

	template<typename T, typename Allocator = std::allocator<T>>
	class vector {
	public:
		using value_type = T;
		using allocator_type = Allocator;
		// ====================================================================
		// =============== 构造 / 析构 / 拷贝 / 移动（五法则）=================
		// ====================================================================
		// 访问权限：public								   生命周期：对象存储期

		// 1. vector()：默认构造函数，初始化为空 vector
		// 调用时机：当用户创建一个 vector 对象时，默认构造函数会被调用，初始化一个空的 vector。
		constexpr vector() : data_(nullptr), size_(0), capacity_(0) {}

		// 2. vector(std::size_t n)：构造函数，创建 n 个默认初始化的元素（强异常安全）
		// 调用时机：当用户希望创建一个指定大小的 vector 时，使用此构造函数。
		explicit constexpr vector(std::size_t n) : vector() {
			if (n > 0) {	// 如果 n 大于 0，则分配内存并构造元素
				data_ = alloc_.allocate(n); // 分配 n 个元素的内存
				try {
					std::uninitialized_value_construct_n(data_, n); // 使用 std::uninitialized_value_construct_n 进行默认初始化
					size_ = n; // 设置有效长度
					capacity_ = n; // 设置容量
				}
				catch (...) {
					alloc_.deallocate(data_, n); // 如果构造过程中抛出异常，释放已分配的内存
					data_ = nullptr; // 重置指针为 nullptr
					throw;
				}
			}
		}

		// 3. vector(std::size_t n, value_type val)：构造函数，创建 n 个值为 val 的元素（强异常安全）
		// 调用时机：当用户希望创建一个指定大小个相同元素的 vector 时，调用此函数
		constexpr vector(std::size_t n, value_type val) : vector() {
			if (n > 0) {
				data_ = alloc_.allocate(n); // 分配 n 个元素内存
				try {
					std::uninitialized_fill_n(data_, n, val);
					size_ = n;
					capacity_ = n;
				}
				catch (...) {
					alloc_.deallocate(data_, n);
					data_ = nullptr;
					throw;
				}
			}
		}

		// 4. vector(std::initializer_list<value_type> list)：使用 std::initializer_list 进行初始化（强异常安全）
		// 调用时机：当用户希望使用列表初始化一个 vector 时，使用此构造函数。
		// 使用 std::uninitialized_copy 进行拷贝
		constexpr vector(std::initializer_list<value_type> list) : vector() {
			if (list.size() > 0) {
				data_ = alloc_.allocate(list.size());	// 分配内存
				try {
					std::uninitialized_copy(list.begin(), list.end(), data_);	// 使用 std::uninitialized_copy 进行拷贝
					size_ = list.size();	// 设置有效长度
					capacity_ = list.size();	// 设置容量
				}
				catch (...) {	// 如果拷贝过程中抛出异常，释放已分配的内存
					alloc_.deallocate(data_, list.size());
					data_ = nullptr;
					throw;	// 重新抛出异常
				}
			}
		}

		// 5. vector(const vector& rhs)：拷贝构造函数，使用 std::uninitialized_copy_n 进行拷贝（强异常安全 && 深拷贝）
		// 调用时机：当用户希望创建一个 vector 的副本时，使用此拷贝构造函数。
		constexpr vector(const vector& rhs) : vector() {
			if (rhs.size_ > 0) {
				data_ = alloc_.allocate(rhs.size_);
				try {
					std::uninitialized_copy_n(rhs.data_, rhs.size_, data_);		// 拷贝构造函数，使用 std::uninitialized_copy_n 进行拷贝
					size_ = rhs.size_;
					capacity_ = rhs.size_;
				}
				catch (...) {			// 如果拷贝过程中抛出异常，释放已分配的内存
					alloc_.deallocate(data_, rhs.size_);
					data_ = nullptr;
					throw;	// 重新抛出异常
				}
			}
		}

		// 6. vector& operator=(const vector& rhs)：拷贝赋值运算符，使用拷贝构造函数和 swap 实现（强异常安全）
		// 调用时机：当用户希望将一个 vector 的内容赋值给另一个 vector 时，使用此拷贝赋值运算符。
		vector& operator=(const vector& rhs) {
			if (this == &rhs) return *this;
			vector tmp(rhs);	// 使用拷贝构造函数创建临时对象
			swap(tmp);			// 交换当前对象与临时对象的内容
			return *this;
		}

		// 7. vector(vector&& rhs) noexcept：移动构造函数，使用 std::move 进行移动（浅拷贝）
		// 调用时机：当用户希望将一个临时 vector 的内容移动到另一个 vector 时，使用此移动构造函数。
		vector(vector&& rhs) noexcept {
			move_from(std::move(rhs));
		}

		// 8. vector& operator=(vector&& rhs) noexcept：移动赋值运算符，使用 std::move 进行移动（浅拷贝）
		// 调用时机：当用户希望将一个临时 vector 的内容移动赋值给另一个 vector 时，使用此移动赋值运算符。
		vector& operator=(vector&& rhs) noexcept {
			if (this != &rhs)  move_from(std::move(rhs));
			return *this;
		}

		// 9. ~vector()：析构函数，释放内存，销毁 data_ 数组（强异常安全）
		// 调用时机：当 vector 对象生命周期结束时，析构函数会被调用，释放资源。
		~vector() {
			if (capacity_ == 0) return;		// 如果容量为 0，则无需释放内存
			clear_and_deallocate();			// 调用 clear_and_deallocate 清空元素释放内存
		}

	public:
		// =============================================================
		// ====================== 迭代器支持 ===========================
		// =============================================================
		// 访问权限：public                         生命周期：对象存储期

		// 迭代器类型定义，使用原生指针作为迭代器
		using iterator = value_type*;
		using const_iterator = const value_type*;

		// 迭代器接口函数，返回指向首元素和尾元素的指针，支持范围 for 循环
		// 1. begin()：返回指向首元素的迭代器
		iterator begin() noexcept { return data_; }
		// 2. cbegin()：返回指向首元素的常量迭代器
		const_iterator begin() const noexcept { return data_; }
		// 3. cbegin()：返回指向首元素的常量迭代器
		const_iterator cbegin() const noexcept { return data_; }
		// 4. end()：返回指向尾元素的迭代器
		iterator end() noexcept { return data_ + size_; }
		// 5. cend()：返回指向尾元素的常量迭代器
		const_iterator end() const noexcept { return data_ + size_; }
		// 6. cend()：返回指向尾元素的常量迭代器
		const_iterator cend() const noexcept { return data_ + size_; }

	public:
		// =============================================================
		// ================== 该处为修改器接口函数 =====================
		// =============================================================
		// 访问权限：public                         生命周期：对象存储期

		// 1. insert()：在指定位置插入一个元素
		void insert(std::size_t idx, value_type val) {
			if (idx > size_) throw std::out_of_range("Index out of range");
			if (size_ == capacity_) reallocate(capacity_ == 0 ? 1 : capacity_ * 2);
			std::move_backward(data_ + idx, data_ + size_, data_ + size_ + 1); // 移动元素
			std::allocator_traits<Allocator>::construct(alloc_, data_ + idx, val); // 构造新元素
			++size_;
		}

		// 2. erase()：删除指定位置的元素
		void erase(std::size_t idx) {
			if (size_ == 0) throw std::out_of_range("Erase from empty vector");
			if (idx >= size_) throw std::out_of_range("Index out of range");
			std::move(data_ + idx + 1, data_ + size_, data_ + idx); // 移动元素
			std::allocator_traits<Allocator>::destroy(alloc_, data_ + size_ - 1);
			--size_;
		}

		// 3. at()：访问指定位置的元素，带边界检查
		value_type& at(std::size_t idx) {
			if (idx >= size_) throw std::out_of_range("Index out of range");
			return data_[idx];
		}

		// 4. assign()：重新赋值，清空当前元素并拷贝新元素
		void assign(std::initializer_list<value_type> list) {
			clear(); // 清空当前元素
			if (list.size() > capacity_) reallocate(list.size()); // 如果新元素数量大于当前容量，则重新分配内存
			std::uninitialized_copy(list.begin(), list.end(), data_); // 使用 std::uninitialized_copy 进行拷贝
			size_ = list.size(); // 更新有效长度
		}

		/// 5. emplace()：在指定位置直接构造一个元素，避免拷贝或移动，提高性能
		void emplace(std::size_t idx, value_type val) {
			insert(idx, val); // 直接调用 insert 实现
		}

		// 6.clear()：清空所有元素，但不改变容量（修改器函数）
		constexpr void clear() noexcept {
			for (std::size_t i = 0; i < size_; ++i) std::destroy_at(data_ + i);
			size_ = 0;
		}

		// 7. swap()：交换两个 vector 对象的内容，使用 std::swap 交换指针和大小
		void swap(vector& other) noexcept {
			using std::swap;
			swap(data_, other.data_);
			swap(size_, other.size_);
			swap(capacity_, other.capacity_);
			swap(alloc_, other.alloc_);
		}


	public:
		// =============================================================
		// ============ 该处为末尾添加、删除元素的接口函数 =============
		// =============================================================
		// 访问权限：public                         生命周期：对象存储期

		// 1. emplace_back()：在末尾直接构造一个元素，避免拷贝或移动，提高性能
		void emplace_back(value_type val) {
			push_back(val); // 直接调用 push_back 实现
		}

		// 2. push_back()：在末尾添加一个元素
		void push_back(value_type val) {
			if (size_ == capacity_) reallocate(capacity_ == 0 ? 1 : capacity_ * 2);
			// 使用 allocator_traits::construct 或 placement new 来构造元素，避免对 std::construct_at 的不兼容依赖
			std::allocator_traits<Allocator>::construct(alloc_, data_ + size_, val);
			++size_;
		}

		// 3. pop_back()：删除末尾的元素
		void pop_back() {
			if (size_ == 0) throw std::out_of_range("Pop from empty vector");
			std::allocator_traits<Allocator>::destroy(alloc_, data_ + size_ - 1);
			--size_;
		}

	public:
		// =============================================================
		// =============== 该处为范围操作的接口函数 ====================
		// =============================================================
		// 访问权限：public                         生命周期：对象存储期

		// 1. append_range()：追加一个范围的元素
		void append_range(std::initializer_list<value_type> list) {
			for (const auto& val : list) push_back(val); // 使用 push_back 添加元素，确保容量足够
		}

		// 2. insert_range()：在指定位置插入一个范围的元素
		void insert_range(std::size_t idx, std::initializer_list<value_type> list) {
			if (idx > size_) throw std::out_of_range("Index out of range");
			for (const auto& val : list) insert(idx++, val); // 使用 insert 添加元素，确保容量足够
		}

		// 3. assign_range()：重新赋值一个范围的元素
		void assign_range(std::initializer_list<value_type> list) {
			clear(); // 清空当前元素
			if (list.size() > capacity_) reallocate(list.size()); // 如果新元素数量大于当前容量，则重新分配内存
			std::uninitialized_copy(list.begin(), list.end(), data_); // 使用 std::uninitialized_copy 进行拷贝
			size_ = list.size(); // 更新有效长度
		}

		// 4. erase_range()：删除指定范围的元素（原创，非 STL 接口）
		void erase_range(std::size_t idx, std::size_t count) {
			if (idx >= size_ || idx + count > size_) throw std::out_of_range("Index out of range");
			for (std::size_t i = 0; i < count; ++i) {
				std::allocator_traits<Allocator>::destroy(alloc_, data_ + idx + i);
			}
			std::move(data_ + idx + count, data_ + size_, data_ + idx); // 移动元素
			size_ -= count;
		}

	public:
		// =============================================================
		// ================= 该处为容量操作的接口函数 ==================
		// =============================================================
		// 访问权限：public                         生命周期：对象存储期

		// 1. reserve()：预留容量，若 new_capacity 小于当前容量，则不做任何操作
		void reserve(std::size_t new_capacity) {
			if (new_capacity > capacity_) reallocate(new_capacity);		// 仅在新容量大于当前容量时才重新分配内存
		}

		// 2. resize()：改变有效长度，若 new_size 小于当前有效长度，则销毁多余元素
		// 若 new_size 大于当前有效长度，则在末尾添加元素，值为 val
		void resize(std::size_t new_size, value_type val = value_type{}) {
			if (new_size < size_) {	// 如果新大小小于当前大小，则需要销毁多余元素
				for (std::size_t i = new_size; i < size_; ++i)		// 销毁多余元素
					std::allocator_traits<Allocator>::destroy(alloc_, data_ + i);
			}
			else if (new_size > size_) {	// 如果新大小大于当前大小，则需要构造新元素
				if (new_size > capacity_) reallocate(new_size); // 如果新大小大于当前容量，则重新分配内存
				for (std::size_t i = size_; i < new_size; ++i)	// 构造新元素
					std::allocator_traits<Allocator>::construct(alloc_, data_ + i, val);
			}
			size_ = new_size;	// 更新有效长度
		}

		// 3. shrink_to_fit()：收缩容量，使容量等于有效长度（原创，非 STL 接口）
		void shrink_to_fit() {
			if (size_ < capacity_) reallocate(size_);	// 仅在有效长度小于容量时才重新分配内存
		}

	public:
		// =============================================================
		// ===================== 该处为访问器函数 ======================
		// =============================================================
		// 访问权限：public                         生命周期：对象存储期

		// 1. size()：返回有效长度
		constexpr std::size_t size() const noexcept { return size_; }
		// 2. capacity()：返回容量
		constexpr std::size_t capacity() const noexcept { return capacity_; }
		// 3. data()：返回数据指针（返回首地址），支持范围 for 循环
		constexpr value_type* data() const noexcept { return data_; }
		// 4. get_allocator()：返回内存分配器
		constexpr Allocator get_allocator() const noexcept { return alloc_; }
		// 5. empty()：判断是否为空
		constexpr bool empty() const noexcept { return size_ == 0; }
		// 6. front()：返回首元素的引用，若为空则抛出异常
		value_type& front() {
			if (empty()) throw std::out_of_range("Accessing front of empty vector");
			return data_[0];
		}
		// 7. back()：返回末尾元素的引用，若为空则抛出异常
		value_type& back() {
			if (empty()) throw std::out_of_range("Accessing back of empty vector");
			return data_[size_ - 1];
		}
		// 7. front()：重载
		const value_type& front() const {
			if (empty()) throw std::out_of_range("Accessing front of empty vector");
			return data_[0];
		}
		// 8. back()：重载
		const value_type& back() const {
			if (empty()) throw std::out_of_range("Accessing back of empty vector");
			return data_[size_ - 1];
		}

	public:
		// =============================================================
		// ======================= 该处重载运算符 ======================
		// =============================================================
		// 访问权限：public                         生命周期：对象存储期

		// ======================= 1.下标运算符重载 ====================== 
		// 重载下标运算符，返回引用，支持链式调用，允许修改元素
		// 注意：此处不进行边界检查，若访问越界会导致未定义行为
		value_type& operator[](std::size_t idx) {
			if (idx >= size_) throw std::out_of_range("Index out of range"); // 检查索引是否越界
			return data_[idx];
		}

		// 重载下标运算符，返回常量引用，支持链式调用，允许只读访问元素
		// 注意：此处不进行边界检查，若访问越界会导致未定义行为
		const value_type& operator[](std::size_t idx) const {
			if (idx >= size_) throw std::out_of_range("Index out of range"); // 检查索引是否越界
			return data_[idx];
		}

	private:
		// =============================================================
		// ====================== 私有辅助函数 =========================
		// =============================================================
		// 访问权限：private                        生命周期：对象存储期

		// 1. reallocate()：vector 内部的私有成员函数，用于重新分配内存，扩容为 new_capacity
		void reallocate(std::size_t new_capacity) {
			if (new_capacity <= capacity_) return; // 如果新容量小于等于当前容量，则无需重新分配内存
			value_type* new_data = alloc_.allocate(new_capacity);	// 分配新内存
			try {
				std::uninitialized_move_n(data_, size_, new_data); // 将旧数据移动到新内存中，使用 std::uninitialized_move_n 进行移动
			}
			catch (...) {
				alloc_.deallocate(new_data, new_capacity);	// 如果移动过程中抛出异常，释放新分配的内存
				throw;	// 重新抛出异常
			}

			for (std::size_t i = 0; i < size_; ++i)	std::destroy_at(data_ + i); // 销毁旧数据
			if (capacity_ > 0)	alloc_.deallocate(data_, capacity_);			// 释放旧内存

			data_ = new_data;
			capacity_ = new_capacity;
		}

		// 2. clear_and_deallocate() noexcept：清空所有元素然后释放内存
		void clear_and_deallocate() noexcept {
			for (std::size_t i = 0; i < size_; i++) std::destroy_at(data_ + i);
			alloc_.deallocate(data_, capacity_);	// 释放内存，销毁 data_ 数组
		}

		// 3. move_form(vector&& rhs)：移动辅助函数
		void move_from(vector&& rhs) {
			// 释放当前对象的资源
			data_ = rhs.data_;
			size_ = rhs.size_;
			capacity_ = rhs.capacity_;

			// 将 rhs 的资源置为空，防止析构时释放资源
			rhs.data_ = nullptr;
			rhs.size_ = 0;
			rhs.capacity_ = 0;
		}

	private:
		// =============================================================
		// ====================== 类的私有成员变量 =====================
		// =============================================================
		// 访问权限：private                        生命周期：对象存储期

		value_type* data_{};				// 数据数组 
		std::size_t size_{};				// 有效长度
		std::size_t capacity_{};			// 容器容量
		Allocator alloc_;					// 内存分配器
	};

	// =============================================================
	// ===================== 非成员比较运算符 ======================
	// =============================================================
	// 访问权限：public                         生命周期：静态存储期

	// 需要拷贝两次 vector 对象不如 friend 函数直接访问私有成员变量，效率更高
	// 但为了遵循 STL 风格，提供非成员函数版本的比较运算符
	// 1. operator==：按字典序比较，先比较元素大小，若相等则比较长度
	template<typename value_type>
	bool operator==(const vector<value_type>& lhs, const vector<value_type>& rhs) {
		if (lhs.size_ != rhs.size_) return false;
		for (std::size_t i = 0; i < lhs.size_; ++i)
			if (lhs.data_[i] != rhs.data_[i]) return false;
		return true;
	}

	// 2. operator<：按字典序比较，先比较元素大小，若相等则比较长度
	template<typename value_type>
	bool operator<(const vector<value_type>& lhs, const vector<value_type>& rhs) {
		std::size_t min_size = std::min(lhs.size_, rhs.size_);
		for (std::size_t i = 0; i < min_size; ++i) {
			if (lhs.data_[i] < rhs.data_[i]) return true;
			if (lhs.data_[i] > rhs.data_[i]) return false;
		}
		return lhs.size_ < rhs.size_;
	}

	// 其他比较运算符可以通过重载 operator== 和 operator< 来实现，避免重复代码
	template<typename value_type>
	bool operator!=(const vector<value_type>& lhs, const vector<value_type>& rhs) { return !(lhs == rhs); }
	template<typename value_type>
	bool operator>(const vector<value_type>& lhs, const vector<value_type>& rhs) { return rhs < lhs; }
	template<typename value_type>
	bool operator<=(const vector<value_type>& lhs, const vector<value_type>& rhs) { return !(lhs > rhs); }
	template<typename value_type>
	bool operator>=(const vector<value_type>& lhs, const vector<value_type>& rhs) { return !(lhs < rhs); }

} // namespace myStd::STL
