#pragma once

struct Bite { Cell* cell; obj_idx food_id; };

struct alignas(64) BiteResolution
{
	struct Iterator
	{

		using iterator_category = std::forward_iterator_tag;
		using value_type = Bite;
		using pointer = const Bite*;
		using reference = const Bite&;
		explicit Iterator(const Bite* ptr) : ptr_(ptr) {}

		reference operator*() const { return *ptr_; }

		Iterator& operator++() { ++ptr_; return *this; }
		Iterator operator++(int) { Iterator tmp = *this; ++ptr_; return tmp; }

		bool operator==(const Iterator& other) const { return ptr_ == other.ptr_; }
		bool operator!=(const Iterator& other) const { return ptr_ != other.ptr_; }

	private:
		const Bite* ptr_;
	};

	explicit BiteResolution(int reserve)
	{
		bites_.resize(reserve);
	}

	void add(const Bite& b)
	{
		assert(size_ < static_cast<int>(bites_.size()) && "BiteResolution capacity exceeded");
		bites_[size_++] = b;
	}


	void clear() { size_ = 0; }

	[[nodiscard]] int  size()     const { return size_; }
	[[nodiscard]] bool empty()    const { return size_ == 0; }
	[[nodiscard]] int  capacity() const { return static_cast<int>(bites_.size()); }

	[[nodiscard]] Bite operator[](int i) const { assert(i >= 0 && i < size_); return bites_[i]; }

	Iterator begin() const { return Iterator(bites_.data()); }
	Iterator end()   const { return Iterator(bites_.data() + size_); }

private:
	std::vector<Bite> bites_{};
	int size_ = 0;
};