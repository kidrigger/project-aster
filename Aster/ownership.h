// =============================================
//  Aster: ownership.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once
#include <type_traits>
#include <utility>

template <typename T>
concept Destroyable = requires(T _t) {
	{ _t.destroy() };
};

#define ENABLE_BORROW_CHECK

template <typename T>
class Borrowed {

public:
	using handle_type = std::decay_t<T>;

	Borrowed() : Borrowed(nullptr) {}
	explicit Borrowed(nullptr_t) : handle_{ nullptr } {}
	explicit Borrowed(handle_type* const& _ptr) : handle_{ _ptr } {}
#if defined(ENABLE_BORROW_CHECK)
	explicit Borrowed(handle_type* const& _ptr, const std::shared_ptr<i32>& _bc) : handle_{ _ptr }
	                                                                             , borrow_count_{ _bc } {}
#endif // defined(ENABLE_BORROW_CHECK)

	Borrowed(const Borrowed& _other)
		: handle_{ _other.handle_ }
#if defined(ENABLE_BORROW_CHECK)
		, borrow_count_{ _other.borrow_count_ }
#endif // defined(ENABLE_BORROW_CHECK)
	{}

	Borrowed(Borrowed&& _other) noexcept
		: handle_{ _other.handle_ }
#if defined(ENABLE_BORROW_CHECK)
		, borrow_count_{ std::move(_other.borrow_count_) }
#endif // defined(ENABLE_BORROW_CHECK)
	{}

	Borrowed& operator=(const Borrowed& _other) {
		if (this == &_other) return *this;
		handle_ = _other.handle_;
#if defined(ENABLE_BORROW_CHECK)
		borrow_count_ = _other.borrow_count_;
#endif // defined(ENABLE_BORROW_CHECK)
		return *this;
	}

	Borrowed& operator=(Borrowed&& _other) noexcept {
		if (this == &_other) return *this;
		handle_ = _other.handle_;
#if defined(ENABLE_BORROW_CHECK)
		std::swap(borrow_count_, _other.borrow_count_);
#endif // defined(ENABLE_BORROW_CHECK)
		return *this;
	}

	handle_type* operator->() {
		ERROR_IF(!valid(), "Null pointer");
		return handle_;
	}

	handle_type& operator*() {
		ERROR_IF(!valid(), "Null pointer");
		return *handle_;
	}

	handle_type const* operator->() const {
		ERROR_IF(!valid(), "Null pointer");
		return handle_;
	}

	handle_type const& operator*() const {
		ERROR_IF(!valid(), "Null pointer");
		return *handle_;
	}

	auto operator<=>(const Borrowed& _other) const {
		return handle_ <=> _other.handle_;
	}

	b8 valid() const {
		return handle_ != nullptr;
	}

	operator b8() const {
		return valid();
	}

	~Borrowed() {
		handle_ = nullptr;
	}

private:
	handle_type* handle_;

#if defined(ENABLE_BORROW_CHECK)
	std::shared_ptr<i32> borrow_count_{};
#endif // defined(ENABLE_BORROW_CHECK)
};

template <typename T>
class Owned {
public:
	using handle_type = std::decay_t<T>;

	Owned() : Owned(nullptr) {}
	explicit Owned(nullptr_t) : handle_{ nullptr } {}
	explicit Owned(handle_type* const& _ptr) : handle_(_ptr) {}
	Owned(handle_type*&& _ptr) : handle_(_ptr) {}

	Owned(const Owned& _other) = delete;

	Owned(Owned&& _other) noexcept
		: handle_{ std::exchange(_other.handle_, nullptr) }
#if defined(ENABLE_BORROW_CHECK)
		, borrow_count_{ std::move(_other.borrow_count_) }
#endif // defined(ENABLE_BORROW_CHECK)
	{}

	Owned& operator=(const Owned& _other) = delete;

	Owned& operator=(Owned&& _other) noexcept {
		if (this == &_other) return *this;
		std::swap(handle_, _other.handle_);
#if defined(ENABLE_BORROW_CHECK)
		std::swap(borrow_count_, _other.borrow_count_);
#endif // defined(ENABLE_BORROW_CHECK)
		return *this;
	}

	handle_type* operator->() {
		ERROR_IF(!valid(), "Null pointer");
		return handle_;
	}

	handle_type& operator*() {
		ERROR_IF(!valid(), "Null pointer");
		return *handle_;
	}

	auto operator<=>(const Owned& _other) const {
		ERROR_IF(!valid(), "Null pointer");
		return handle_ <=> _other.handle_;
	}

	[[nodiscard]]
	Borrowed<T> borrow() {
		ERROR_IF(!valid(), "Can't borrow from a null");
#if defined(ENABLE_BORROW_CHECK)
		return Borrowed<T>{ handle_, borrow_count_ };
#else
		return Borrowed<T>{ handle_ };
#endif // defined(ENABLE_BORROW_CHECK)
	}

	[[nodiscard]]
	b8 valid() const {
		return handle_ != nullptr;
	}

	operator b8() const {
		return valid();
	}

#if defined(ENABLE_BORROW_CHECK)
	[[nodiscard]]
	i32 get_borrow_count() const {
		return borrow_count_.use_count() - 1;
	}
#endif // defined(ENABLE_BORROW_CHECK)

	~Owned() {
		if (!handle_) return;

#if defined(ENABLE_BORROW_CHECK)
		assert(get_borrow_count() <= 0 && "Owner lost during borrow");
#endif // defined(ENABLE_BORROW_CHECK)

		if constexpr (Destroyable<T>) {
			handle_->destroy();
		}
		delete handle_;
	}

private:
	handle_type* handle_;

#if defined(ENABLE_BORROW_CHECK)
	std::shared_ptr<i32> borrow_count_{ new i32{ 42 } };
#endif // defined(ENABLE_BORROW_CHECK)
};

template <typename T, b8 ThreadSafe = false>
class RefCount {
public:
	using handle_type = std::decay_t<T>;
	using counter_type = std::conditional_t<ThreadSafe, std::atomic<u32>, int>;

	RefCount() : RefCount{ nullptr } {}

	explicit RefCount(nullptr_t) : handle_{ nullptr }
	                             , count_{ nullptr } {}

	explicit RefCount(handle_type* const& _ptr)
		: handle_{ _ptr }
		, count_{ new counter_type{ 1 } } {}

	RefCount(handle_type*&& _ptr)
		: handle_{ _ptr }
		, count_{ new counter_type{ 1 } } {}

	RefCount(const RefCount& _other)
		: handle_{ _other.handle_ }
		, count_{ _other.count_ }
#if defined(ENABLE_BORROW_CHECK)
		, borrow_count_{ _other.borrow_count_ }
#endif // defined(ENABLE_BORROW_CHECK)
	{
		if constexpr (ThreadSafe) count_->fetch_add(1);
		else ++*count_;
	}

	RefCount(RefCount&& _other) noexcept
		: handle_{ std::exchange(_other.handle_, nullptr) }
		, count_{ std::exchange(_other.count_, nullptr) }
#if defined(ENABLE_BORROW_CHECK)
		, borrow_count_{ std::exchange(_other.borrow_count_, nullptr) }
#endif // defined(ENABLE_BORROW_CHECK)
	{ }

	RefCount& operator=(const RefCount& _other) {
		if (this == &_other) return *this;
		handle_ = _other.handle_;
		count_ = _other.count_;
#if defined(ENABLE_BORROW_CHECK)
		borrow_count_ = _other.borrow_count_;
#endif // defined(ENABLE_BORROW_CHECK)
		if constexpr (ThreadSafe) count_->fetch_add(1);
		else ++*count_;
		return *this;
	}

	RefCount& operator=(RefCount&& _other) noexcept {
		if (this == &_other) return *this;
		std::swap(handle_, _other.handle_);
		std::swap(count_, _other.count_);
#if defined(ENABLE_BORROW_CHECK)
		std::swap(borrow_count_, _other.borrow_count_);
#endif // defined(ENABLE_BORROW_CHECK)
		return *this;
	}

	[[nodiscard]]
	handle_type* operator->() {
		ERROR_IF(!valid(), "Null pointer");
		return handle_;
	}

	[[nodiscard]]
	handle_type& operator*() {
		ERROR_IF(!valid(), "Null pointer");
		return *handle_;
	}

	auto operator<=>(const RefCount& _other) const {
		return handle_ <=> _other.handle_;
	}

	[[nodiscard]]
	Borrowed<T> borrow() {
		ERROR_IF(!valid(), "Can't borrow from a Null pointer");
		return { handle_ };
	}

	[[nodiscard]]
	b8 valid() const {
		return handle_ != nullptr;
	}

	[[nodiscard]]
	operator b8() const {
		return handle_ != nullptr;
	}

#if defined(ENABLE_BORROW_CHECK)
	[[nodiscard]]
	i32 get_borrow_count() const {
		return borrow_count_.use_count() - 1;
	}
#endif // defined(ENABLE_BORROW_CHECK)

	~RefCount() {
		if (count_) {
			u32 val;
			if constexpr (ThreadSafe) val = count_->fetch_sub(1);
			else val = --*count_;
			if (val == 0) {

#if defined(ENABLE_BORROW_CHECK)
				ERROR_IF(get_borrow_count() != 0, "Owner lost during borrow");
#endif // defined(ENABLE_BORROW_CHECK)

				delete count_;
				if (handle_) {
					if constexpr (Destroyable<T>) {
						handle_->destroy();
					}
					delete handle_;
				}
			}
		}
	}

private:
	handle_type* handle_;
	counter_type* count_;

#if defined(ENABLE_BORROW_CHECK)
	std::shared_ptr<i32> borrow_count_{};
#endif // defined(ENABLE_BORROW_CHECK)
};

template <typename T>
concept HasBorrow = requires (T _t) {
	{ T::handle_type };
	{ _t.borrow() } -> std::same_as<Borrowed<typename T::handle_type>>;
};

template <typename T> requires HasBorrow<T>
[[nodiscard]]
auto borrow(T& _borrowable) {
	return _borrowable.borrow();
}

template <typename T>
[[nodiscard]]
Borrowed<T> borrow(T* const& _ptr) {
	return Borrowed<T>{ _ptr };
}

template <typename T> requires (!HasBorrow<T>) && std::negation_v<std::is_pointer<T>>
[[nodiscard]]
auto borrow(T& _ref) {
	return borrow(&_ref);
}
