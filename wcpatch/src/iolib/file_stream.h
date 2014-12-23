#ifndef IOLIB_FILE_STREAM_INCLUDED
#define IOLIB_FILE_STREAM_INCLUDED
#pragma once

#include "stream.h"
#include "string_view.h"
#include "types.h"

#include <fstream>
#include <cstdint>


namespace iolib {

class file;

class file_error;

class file_input_stream;
class file_output_stream;
class file_stream;

class file
{
public:
	typedef std::fstream::pos_type position_type;

	typedef seekable_base::seek_from seek_from;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;

	enum class mode
	{
		none     = 0x00,
		open     = 0x01,
		create   = 0x02,
		open_or_create = open | create,
		read     = 0x04,
		write    = 0x08,
		at_end   = 0x10,
		append   = 0x20
	};

public:
	file();
	file(const path_char path[], file::mode mode);
	file(file&& other);
	file& operator = (file&& other);
	~file();
private:
	file(const file&);
	file& operator = (const file&);

public:
	position_type position() const;
	void set_position(position_type pos);
	void seek(difference_type distance, seek_from from = seek_from::current);
	bool at_end() const;

	size_type read(void* buffer, size_type size);
	void write(const void* buffer, size_type size);

private:
	std::filebuf handle;
};

flags_enum(file::mode)

class file_error : public std::system_error
{
public:
	explicit file_error(const std::string& msg, const std::error_code& ec = std::io_errc::stream);
	explicit file_error(const char* msg, const std::error_code& ec = std::io_errc::stream);
};

namespace detail
{
	template <class Stream, class Base>
	class file_stream_base : public Base
	{
	protected:
		file_stream_base() { }
		~file_stream_base() override { }

	public:
		bool at_end() const { return This()->f.at_end(); }

	private:
		void do_seek(typename Base::difference_type distance, typename Base::seek_from from) override { This()->f.seek(distance, from); }

		//constexpr bool file_position_requires_alloc() { return !std::is_pod<file::position_type>::value || sizeof(file::position_type) > sizeof(uintptr_t); }
	#define file_position_requires_alloc() (!std::is_pod<file::position_type>::value || sizeof(file::position_type) > sizeof(uintptr_t))

		uintptr_t create_position_handle() const override {	return create_position_handle(is_true<file_position_requires_alloc()>()); }
		uintptr_t copy_position_handle(uintptr_t handle) const override { return copy_position_handle(handle, is_true<file_position_requires_alloc()>()); }
		void      release_position_handle(uintptr_t handle) const override { return release_position_handle(handle, is_true<file_position_requires_alloc()>()); }
		void      set_position_from_handle(uintptr_t handle) override { return set_position_from_handle(handle, is_true<file_position_requires_alloc()>()); }

	#undef file_position_requires_alloc

		uintptr_t create_position_handle(std::true_type allocate) const;
		uintptr_t copy_position_handle(uintptr_t handle, std::true_type allocate) const;
		void      release_position_handle(uintptr_t handle, std::true_type allocate) const;
		void      set_position_from_handle(uintptr_t handle, std::true_type allocate);

		uintptr_t create_position_handle(std::false_type allocate) const;
		uintptr_t copy_position_handle(uintptr_t handle, std::false_type allocate) const;
		void      release_position_handle(uintptr_t handle, std::false_type allocate) const;
		void      set_position_from_handle(uintptr_t handle, std::false_type allocate);

	private:
		Stream* This() { return static_cast<Stream*>(this); }
		const Stream* This() const { return static_cast<const Stream*>(this); }
	};
}

class file_input_stream : public detail::file_stream_base<file_input_stream, seekable_input_stream>
{
public:
	file_input_stream() { }
	explicit file_input_stream(const path_char path[]) : f(path, file::mode::open | file::mode::read) { }
	file_input_stream(file_input_stream&& other) : f(std::move(other.f)) { }
	file_input_stream& operator = (file_input_stream&& other) { f = std::move(other.f); return *this; }

private:
	size_type do_read(void* buffer, size_type size) override { return f.read(buffer, size); }

private:
	friend class detail::file_stream_base<file_input_stream, seekable_input_stream>;
	file f;
};

class file_output_stream : public detail::file_stream_base<file_output_stream, seekable_output_stream>
{
public:
	file_output_stream() { }
	explicit file_output_stream(const path_char path[]) : f(path, file::mode::open_or_create | file::mode::write) { }
	file_output_stream(file_output_stream&& other) : f(std::move(other.f)) { }
	file_output_stream& operator = (file_output_stream&& other) { f = std::move(other.f); return *this; }

private:
	void do_write(const void* buffer, size_type size) override { f.write(buffer, size); }

private:
	friend class detail::file_stream_base<file_output_stream, seekable_output_stream>;
	file f;
};

class file_stream : public detail::file_stream_base<file_stream, seekable_stream>
{
public:
	file_stream() { }
	explicit file_stream(const path_char path[]) : f(path, file::mode::open_or_create | file::mode::read | file::mode::write) { }
	file_stream(file_stream&& other) : f(std::move(other.f)) { }
	file_stream& operator = (file_stream&& other) { f = std::move(other.f); return *this; }

private:
	size_type do_read(void* buffer, size_type size) override { return f.read(buffer, size); }
	void do_write(const void* buffer, size_type size) override { f.write(buffer, size); }

private:
	friend class detail::file_stream_base<file_stream, seekable_stream>;
	file f;
};

template <class Stream, class Base>
uintptr_t detail::file_stream_base<Stream, Base>::create_position_handle(std::true_type) const
{
	return reinterpret_cast<uintptr_t>(new file::position_type(This()->f.position()));
}

template <class Stream, class Base>
uintptr_t detail::file_stream_base<Stream, Base>::copy_position_handle(uintptr_t handle, std::true_type) const
{
	return reinterpret_cast<uintptr_t>(new file::position_type(*reinterpret_cast<file::position_type*>(handle)));
}

template <class Stream, class Base>
void detail::file_stream_base<Stream, Base>::release_position_handle(uintptr_t handle, std::true_type) const
{
	delete reinterpret_cast<file::position_type*>(handle);
}

template <class Stream, class Base>
void detail::file_stream_base<Stream, Base>::set_position_from_handle(uintptr_t handle, std::true_type)
{
	This()->f.set_position(*reinterpret_cast<file::position_type*>(handle));
}

template <class Stream, class Base>
uintptr_t detail::file_stream_base<Stream, Base>::create_position_handle(std::false_type) const
{
	union { file::position_type position; uintptr_t handle; } adapter;
	adapter.handle = 0;
	adapter.position = This()->f.position();
	return adapter.handle;
}

template <class Stream, class Base>
uintptr_t detail::file_stream_base<Stream, Base>::copy_position_handle(uintptr_t handle, std::false_type) const
{
	return handle;
}

template <class Stream, class Base>
void detail::file_stream_base<Stream, Base>::release_position_handle(uintptr_t handle, std::false_type) const
{
}

template <class Stream, class Base>
void detail::file_stream_base<Stream, Base>::set_position_from_handle(uintptr_t handle, std::false_type)
{
	union { file::position_type position; uintptr_t handle; } adapter;
	adapter.handle = handle;
	This()->f.set_position(adapter.position);
}

} // namespace iolib

#endif