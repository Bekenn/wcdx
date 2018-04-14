#ifndef IOLIB_STREAM_INCLUDED
#define IOLIB_STREAM_INCLUDED
#pragma once

#include "error.h"

#include <iterator>
#include <type_traits>
#include <utility>

#include <cstddef>


namespace iolib {

class stream_base;
class seekable_base;

class input_stream;
class output_stream;
class stream;

template <typename POD>
class input_stream_iterator;
template <typename POD>
class output_stream_iterator;

class seekable_input_stream;
class seekable_output_stream;
class seekable_stream;

#if 0
// Maybe do these eventually.  In order to behave properly, each iterator object would have to contain the
// stream as a member (rather than referencing the stream via a pointer).  This implies making (some) seekable
// streams copyable, which may not be unreasonable.  Note that if the iterator provides an accessor for the
// underlying stream, this would enable applications to "advance" the iterator without performing operations
// on the iterator itself.  If this is unacceptable, then these can't derive from *_stream_iterator.
template <class SeekableInputStream, typename POD>
class seekable_input_stream_iterator;
template <class SeekableOutputStream, typename POD>
class seekable_output_stream_iterator;
#endif

class memory_input_stream;
class memory_output_stream;
class memory_stream;

class stream_error;
class stream_read_error;
class stream_write_error;

template <typename POD>
input_stream& operator >> (input_stream& stream, POD& pod);
template <typename POD>
output_stream& operator << (output_stream& stream, const POD& pod);

template <typename POD>
bool operator == (const input_stream_iterator<POD>& a, const input_stream_iterator<POD>& b);
template <typename POD>
bool operator != (const input_stream_iterator<POD>& a, const input_stream_iterator<POD>& b);
template <typename POD>
bool operator == (const output_stream_iterator<POD>& a, const output_stream_iterator<POD>& b);
template <typename POD>
bool operator != (const output_stream_iterator<POD>& a, const output_stream_iterator<POD>& b);


class stream_base
{
public:
    typedef std::size_t size_type;

protected:
    stream_base() { }
    virtual ~stream_base() = 0;
private:
    stream_base(const stream_base&);
    stream_base& operator = (const stream_base&);

public:
    virtual bool at_end() const = 0;
};

class input_stream : public virtual stream_base
{
public:
    /// \brief Reads a POD value as a sequence of bytes from the stream.
    /// \param[out] pod The value read from the stream.
    /// \throws end_of_stream_error if the stream contains more data but does not contain enough data to fill the
    ///                             pod.
    /// \throws stream_read_error   if an error is encountered while reading data from the stream.
    /// \returns \c true if successful or \c false if the stream contains no more data.
    template <typename POD>
    typename std::enable_if<std::is_pod<POD>::value, bool>::type
        read(POD& pod);

    /// \brief Reads a POD value as a sequence of bytes from the stream.
    /// \throws end_of_stream_error if the end of the stream is reached before enough data has been read.
    /// \throws stream_read_error   if an error is encountered while reading data from the stream.
    /// \returns \c true if successful or \c false if the stream contains no more data.
    template <typename POD>
    typename std::enable_if<std::is_pod<POD>::value, POD>::type
        read();

    /// \brief Reads up to \a size objects from \a buffer.
    /// \param[out] buffer Destination buffer where objects will be written.  Must not be \c nullptr if \a size is
    //                     non-zero.
    /// \param[in]  size   Requested size of data to be read from the stream.  The \a buffer argument must point
    ///                    to a writable region of memory large enough to hold this many objects.
    /// \throws end_of_stream_error if the end of the stream is reached before \a size objects have been read and
    ///                             the end of the stream does not lie on an object boundary.
    /// \throws stream_read_error   if an error is encounterd while reading data from the stream.
    /// \returns The number of objects successfully read into \a buffer.  This will be 0 if \a size is 0; otherwise,
    ///          the result will be a nonzero value less than or equal to \a size.
    template <typename POD>
    typename std::enable_if<std::is_pod<POD>::value, size_type>::type
        read(POD* buffer, size_t size);

private:
    /// \brief Implementation of the read(void*, size_type) function.
    /// \param[out] buffer Destination buffer.  Guaranteed not to be \c nullptr.
    /// \param[in]  size   Number of bytes requested.  Guaranteed to be nonzero.
    /// \throws end_of_stream_error if \a size is non-zero and there is no more data to be read from the stream.
    /// \throws stream_read_error if an error is encountered while reading data from the stream.
    /// \returns The number of bytes read from the stream.  Must not return 0.
    /// \see read(POD*, size_type)
    virtual size_type do_read(void* buffer, size_type size) = 0;
};

class output_stream : public virtual stream_base
{
public:
    /// \brief Writes a POD value as a sequence of bytes into the stream.
    /// \param[in] pod The value to be written into the stream.
    /// \throws stream_write_error if an error is encountered while writing data to the stream.
    template <typename POD>
    typename std::enable_if<std::is_pod<POD>::value>::type
        write(const POD& pod);

    /// \brief Writes \a size objects from \a buffer into the stream.
    /// \param[in] buffer Pointer to the objects to be written.  Must not be \c nullptr if size is non-zero.
    /// \param[in] size   Size of the data to be written.  The \a buffer argument must point to a region of
    ///                   memory holding at least this many objects.
    /// \throws stream_write_error if an error is encountered while writing data to the stream.
    template <typename POD>
    typename std::enable_if<std::is_pod<POD>::value>::type
        write(const POD* buffer, size_type size);

private:
    /// \brief Implementation of the write(const void*, size_type) function.
    /// \param[in] buffer Source buffer.  Guaranteed not to be \c nullptr.
    /// \param[in] size   The number of bytes to be written into the stream.
    /// \throws stream_write_error if an error is encountered while writing data to the stream.
    virtual void do_write(const void* buffer, size_type size) = 0;
};


class stream : public virtual input_stream, public virtual output_stream
{
};

template <typename POD>
class input_stream_iterator : public std::iterator<std::input_iterator_tag, POD, std::ptrdiff_t, const POD*, const POD&>
{
public:
    typedef const POD& reference;
    typedef const POD* pointer;

public:
    explicit input_stream_iterator(input_stream* stream = nullptr) : s(stream) { if (stream != nullptr) stream->read(current); }
    reference operator *  () const    { return current; }
    pointer   operator -> () const    { return &current; }
    input_stream_iterator& operator ++ ()    { if (!s->read(current)) s = nullptr; return *this; }
    input_stream_iterator  operator ++ (int) { auto i = *this; ++*this; return i; }

    input_stream* stream() { return s; }

private:
    friend bool operator == <POD> (const input_stream_iterator<POD>& a, const input_stream_iterator<POD>& b);
    input_stream* s;
    POD current;
};

template <typename POD>
class output_stream_iterator : public std::iterator<std::output_iterator_tag, void, void, void, void>
{
public:
    explicit output_stream_iterator(output_stream* stream = nullptr) : s(stream) { }
    output_stream_iterator& operator * () { return *this; }
    output_stream_iterator& operator ++ () { return *this; }
    output_stream_iterator& operator ++ (int) { return *this; }
    output_stream_iterator& operator = (const POD& pod) { s->write(pod); return *this; }

    output_stream* stream() { return s; }

private:
    friend bool operator == <POD> (const output_stream_iterator<POD>& a, const output_stream_iterator<POD>& b);
    output_stream* s;
};


class seekable_base
{
public:
    typedef std::ptrdiff_t difference_type;

    class position_type;

    enum class seek_from
    {
        current,
        beginning,
        end
    };

protected:
    seekable_base() { }
public:
    virtual ~seekable_base() = 0;
private:
    seekable_base(const seekable_base&);
    seekable_base& operator = (const seekable_base&);

public:
    // Possibly should remove these absolute positioning methods, as an appropriate
    // position type can't really be determined at this stage of compilation.  What
    // exists here isn't really adequate, as it may involve undesired allocations.
    // Perhaps streams should instead provide an accessor to the underlying storage
    // object (e.g., a file), which would allow the user to access that object's
    // absolute positioning mechanism (if any).
    position_type position() const;
    void set_position(position_type pos);

    // This one's good, though.
    void seek(difference_type distance, seek_from from = seek_from::current);

protected:
    static const char ErrorMessage_NoSeek[];
    static const char ErrorMessage_PositionNotFullyImplemented[];
    static const char ErrorMessage_InvalidSeek[];

private:
    virtual void do_seek(difference_type distance, seek_from from) { throw invalid_operation(ErrorMessage_NoSeek); }

    virtual uintptr_t create_position_handle() const                  { throw not_implemented(ErrorMessage_PositionNotFullyImplemented); }
    virtual uintptr_t copy_position_handle(uintptr_t handle) const    { throw not_implemented(ErrorMessage_PositionNotFullyImplemented); }
    virtual void      release_position_handle(uintptr_t handle) const { }
    virtual void      set_position_from_handle(uintptr_t handle)      { throw not_implemented(ErrorMessage_PositionNotFullyImplemented); }
};

class seekable_base::position_type
{
public:
    position_type() : owner(nullptr) { }
private:
    position_type(const seekable_base& owner, uintptr_t handle) : owner(&owner), handle(handle) { }
public:
    position_type(const position_type& other) : owner(other.owner), handle(owner->copy_position_handle(other.handle)) { }
    position_type(position_type&& other) : owner(std::move(other.owner)), handle(std::move(other.handle)) { other.owner = nullptr; }
    position_type& operator = (const position_type& other)
    {
        release_handle();
        owner = other.owner;
        handle = owner->copy_position_handle(other.handle);
        return *this;
    }
    position_type& operator = (position_type&& other)
    {
        release_handle();
        owner = std::move(other.owner);
        handle = std::move(other.handle);
        other.owner = nullptr;
        return *this;
    }
    ~position_type() { release_handle(); }

private:
    void release_handle()
    {
        if (owner != nullptr)
            owner->release_position_handle(handle);

        owner = nullptr;
    }

private:
    friend class seekable_base;
    const seekable_base* owner;
    uintptr_t handle;
};

class seekable_input_stream : public virtual input_stream, public virtual seekable_base
{
};

class seekable_output_stream : public virtual output_stream, public virtual seekable_base
{
};

class seekable_stream : public seekable_input_stream, public seekable_output_stream, public stream
{
};


template <class Stream, class Base>
class memory_stream_base : public Base
{
public:
    bool at_end() const { return This()->current == This()->last; }

private:
    void do_seek(typename Base::difference_type distance, typename Base::seek_from from) override;

    uintptr_t create_position_handle() const override;
    uintptr_t copy_position_handle(uintptr_t handle) const override;
    void      set_position_from_handle(uintptr_t handle) override;

private:
    Stream* This() { return static_cast<Stream*>(this); }
    const Stream* This() const { return static_cast<const Stream*>(this); }
};

class memory_input_stream : public memory_stream_base<memory_input_stream, seekable_input_stream>
{
public:
    memory_input_stream() : first(nullptr), last(nullptr), current(nullptr) { }
    memory_input_stream(const void* buffer, size_type size) : first(static_cast<decltype(first)>(buffer)), last(first + size), current(first) { }
    memory_input_stream(memory_input_stream&& other) : first(std::move(other.first)), last(std::move(other.last)), current(std::move(other.current)) { other.first = other.last = other.current = nullptr; }
    memory_input_stream& operator = (memory_input_stream&& other) { first = std::move(other.first); last = std::move(other.last); current = std::move(other.current); other.first = other.last = other.current = nullptr; return *this; }

private:
    size_type do_read(void* buffer, size_type size) override;

private:
    friend class memory_stream_base<memory_input_stream, seekable_input_stream>;
    const char* first;
    const char* last;
    const char* current;
};

class memory_output_stream : public memory_stream_base<memory_output_stream, seekable_output_stream>
{
public:
    memory_output_stream() : first(nullptr), last(nullptr), current(nullptr) { }
    memory_output_stream(void* buffer, size_type size) : first(static_cast<decltype(first)>(buffer)), last(first + size), current(first) { }
    memory_output_stream(memory_output_stream&& other) : first(std::move(other.first)), last(std::move(other.last)), current(std::move(other.current)) { other.first = other.last = other.current = nullptr; }
    memory_output_stream& operator = (memory_output_stream&& other) { first = std::move(other.first); last = std::move(other.last); current = std::move(other.current); other.first = other.last = other.current = nullptr; return *this; }

private:
    void do_write(const void* buffer, size_type size) override;

private:
    friend class memory_stream_base<memory_output_stream, seekable_output_stream>;
    char* first;
    char* last;
    char* current;
};

class memory_stream : public memory_stream_base<memory_stream, seekable_stream>
{
public:
    memory_stream() : first(nullptr), last(nullptr), current(nullptr) { }
    memory_stream(void* buffer, size_type size) : first(static_cast<decltype(first)>(buffer)), last(first + size), current(first) { }
    memory_stream(memory_stream&& other) : first(std::move(other.first)), last(std::move(other.last)), current(std::move(other.current)) { other.first = other.last = other.current = nullptr; }
    memory_stream& operator = (memory_stream&& other) { first = std::move(other.first); last = std::move(other.last); current = std::move(other.current); other.first = other.last = other.current = nullptr; return *this; }

private:
    size_type do_read(void* buffer, size_type size) override;
    void do_write(const void* buffer, size_type size) override;

private:
    friend class memory_stream_base<memory_stream, seekable_stream>;
    char* first;
    char* last;
    char* current;
};


template <class Stream, class Base>
void memory_stream_base<Stream, Base>::do_seek(typename Base::difference_type distance, typename Base::seek_from from)
{
    decltype(This()->current) p = nullptr;
    switch (from)
    {
    case Base::seek_from::current:   p = This()->current + distance; break;
    case Base::seek_from::beginning: p = This()->first + distance; break;
    case Base::seek_from::end:       p = This()->last + distance; break;
    }

    if ((p < This()->first) || (This()->last < p))
        throw std::invalid_argument(Base::ErrorMessage_InvalidSeek);

    This()->current = p;
}

template <class Stream, class Base>
uintptr_t memory_stream_base<Stream, Base>::create_position_handle() const
{
    return reinterpret_cast<uintptr_t>(This()->current);
}

template <class Stream, class Base>
uintptr_t memory_stream_base<Stream, Base>::copy_position_handle(uintptr_t handle) const
{
    return handle;
}

template <class Stream, class Base>
void memory_stream_base<Stream, Base>::set_position_from_handle(uintptr_t handle)
{
    This()->current = reinterpret_cast<decltype(This()->current)>(handle);
}


class stream_error : public std::runtime_error
{
protected:
    explicit stream_error(const std::string& what_arg) : runtime_error(what_arg) { }
    explicit stream_error(const char* what_arg) : runtime_error(what_arg) { }
};

class stream_read_error : public stream_error
{
public:
    explicit stream_read_error(const std::string& what_arg) : stream_error(what_arg) { }
    explicit stream_read_error(const char* what_arg) : stream_error(what_arg) { }
};

class stream_write_error : public stream_error
{
public:
    explicit stream_write_error(const std::string& what_arg) : stream_error(what_arg) { }
    explicit stream_write_error(const char* what_arg) : stream_error(what_arg) { }
};

class end_of_stream_error : public stream_read_error
{
public:
    end_of_stream_error() : stream_read_error("premature end of stream") { }
    explicit end_of_stream_error(const std::string& what_arg) : stream_read_error(what_arg) { }
    explicit end_of_stream_error(const char* what_arg) : stream_read_error(what_arg) { }
};


inline stream_base::~stream_base() { };

template <typename POD>
typename std::enable_if<std::is_pod<POD>::value, bool>::type
    input_stream::read(POD& pod)
{
    size_type remaining = sizeof(POD);
    auto p = reinterpret_cast<char*>(&pod);
    while (remaining > 0)
    {
        size_type bytes_read = do_read(p, remaining);
        if (bytes_read == 0)
        {
            if (remaining == sizeof(POD))
                return false;
            throw end_of_stream_error();
        }

        remaining -= bytes_read;
        p += bytes_read;
    }

    return true;
}

template <typename POD>
typename std::enable_if<std::is_pod<POD>::value, POD>::type
    input_stream::read()
{
    POD result;
    if (!read(result))
        throw end_of_stream_error();
    return result;
}

template <typename POD>
typename std::enable_if<std::is_pod<POD>::value, input_stream::size_type>::type
    input_stream::read(POD* buffer, size_t size)
{
    if (size == 0)
        return 0;
    if (buffer == nullptr)
        throw std::invalid_argument("buffer must not be nullptr");

    size_type count = 0;
    while (size-- > 0)
    {
        if (!read(*buffer++))
            return count;
        ++count;
    }

    return count;
}

template <typename POD>
typename std::enable_if<std::is_pod<POD>::value>::type
    output_stream::write(const POD& pod)
{
    do_write(&pod, sizeof(POD));
}

template <typename POD>
typename std::enable_if<std::is_pod<POD>::value>::type
    output_stream::write(const POD* buffer, size_type size)
{
    if (buffer == nullptr)
        throw std::invalid_argument("buffer must not be nullptr");

    do_write(buffer, size * sizeof(POD));
}

inline seekable_base::~seekable_base() { }


template <typename POD>
input_stream& operator >> (input_stream& stream, POD& pod)
{
    stream.read(pod);
    return stream;
}

template <typename POD>
output_stream& operator << (output_stream& stream, const POD& pod)
{
    stream.write(pod);
    return stream;
}

template <typename POD>
bool operator == (const input_stream_iterator<POD>& a, const input_stream_iterator<POD>& b)
{
    return a.s == b.s && (a.s == nullptr || a.current == b.current);
}

template <typename POD>
bool operator != (const input_stream_iterator<POD>& a, const input_stream_iterator<POD>& b)
{
    return !(a == b);
}

template <typename POD>
bool operator == (const output_stream_iterator<POD>& a, const output_stream_iterator<POD>& b)
{
    return a.s == b.s;
}

template <typename POD>
bool operator != (const output_stream_iterator<POD>& a, const output_stream_iterator<POD>& b)
{
    return !(a == b);
}

} // namespace iolib

#endif
