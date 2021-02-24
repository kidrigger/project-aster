// =============================================
//  Aster: buffer_writer.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <global.h>

#include <core/device.h>

class BufferWriter {
public:
	explicit BufferWriter() {}

	explicit BufferWriter(Borrowed<Buffer>&& _buffer) : buffer_{ std::move(_buffer) }
	                                                  , parent_device_{ _buffer->parent_device } {
		alignment_ = parent_device_->physical_device_properties.limits.minUniformBufferOffsetAlignment;
	}

	template <typename... Ts>
	usize write(Ts const&... _writes) {
		ERROR_IF(buffer_->memory_usage != vma::MemoryUsage::eCpuToGpu &&
			buffer_->memory_usage != vma::MemoryUsage::eCpuOnly, "Memory is not on CPU so mapping can't be done. Use upload_data");

		const auto mapped_memory = begin_mapping();
		auto write_head = mapped_memory;
		auto written = expand(write_to(&write_head, recast<const void*>(&_writes), sizeof(_writes))...);
		end_mapping();

		return written;
	}


	class BufferWriterOStream {
	public:
		explicit BufferWriterOStream(BufferWriter* _writer) : writer_{ _writer } {
			write_head_ = writer_->begin_mapping();
		}

		BufferWriterOStream(const BufferWriterOStream& _other) = delete;

		BufferWriterOStream(BufferWriterOStream&& _other) noexcept
			: writer_{ std::exchange(_other.writer_, nullptr) }
			, write_head_{ std::exchange(_other.write_head_, nullptr) } {}

		BufferWriterOStream& operator=(const BufferWriterOStream& _other) = delete;

		BufferWriterOStream& operator=(BufferWriterOStream&& _other) noexcept {
			if (this == &_other) return *this;
			writer_ = std::exchange(_other.writer_, nullptr);
			write_head_ = std::exchange(_other.write_head_, nullptr);
			return *this;
		}

		template <typename T> requires std::is_trivially_copyable_v<T> && std::negation_v<std::is_pointer<T>>
		BufferWriterOStream& operator<<(T const& _data) {
			writer_->write_to(&write_head_, recast<const void*>(&_data), sizeof(T));
			return *this;
		}

		~BufferWriterOStream() {
			if (writer_) writer_->end_mapping();
		}

	private:
		BufferWriter* writer_;
		u8* write_head_;
	};

	template <typename T> requires std::is_trivially_copyable_v<T> && std::negation_v<std::is_pointer<T>>
	BufferWriterOStream operator<<(T const& _data) {
		BufferWriterOStream o_stream{ this };
		o_stream << _data;
		return std::move(o_stream);
	}

private:
	Borrowed<Buffer> buffer_;
	Borrowed<Device> parent_device_;
	usize alignment_{ 4 };

	template <typename T> requires std::is_same_v<T, usize>
	static usize expand(T _first) {
		return _first;
	}

	template <typename T, typename... Args> requires std::is_same_v<T, usize>
	static usize expand(T _first, Args ... _args) {
		return _first + expand(_args...);
	}

	static usize expand(...) {
		return 0;
	}

	usize write_to(u8** _ptr, const void* _data, const usize _size) const {
		const auto to_write = closest_multiple(_size, alignment_);
		memcpy(*_ptr, _data, _size);
		*_ptr += to_write;
		return to_write;
	}

	u8* begin_mapping() {
		auto [result, mapped_memory] = parent_device_->allocator.mapMemory(buffer_->allocation);
		ERROR_IF(failed(result), std::fmt("Memory mapping failed with %s", to_cstr(result))) THEN_CRASH(result);

		return cast<u8*>(mapped_memory);
	}

	void end_mapping() {
		parent_device_->allocator.unmapMemory(buffer_->allocation);
	}
};
