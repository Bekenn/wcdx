#include <iolib/file_stream.h>

using namespace iolib;
using namespace std;

file::file() : handle()
{
}

file::file(const path_char path[], file::mode mode) : handle()
{
    ios_base::openmode ios_mode = ios_base::binary;
    if (test_flags(mode, file::mode::read))
    {
        ios_mode |= ios_base::in;
        if (!test_flags(mode, file::mode::open))
            throw file_error("file::mode::read requires file::mode::open", make_error_code(errc::not_supported));
    }

    if (test_flags(mode, file::mode::write))
    {
        ios_mode |= ios_base::out;
        if (!test_flags(mode, file::mode::open_or_create))
        {
            if (test_flags(mode, file::mode::create))
                ios_mode |= ios_base::trunc;
#ifdef _CPPLIB_VER  // Dinkumware
            else if (test_flags(mode, file::mode::open))
                ios_mode |= ios_base::_Nocreate;
            else
                throw file_error("file open mode not set", make_error_code(errc::invalid_argument));
#else
            else
                throw file_error("file open mode is not supported", make_error_code(errc::not_supported));
#endif
        }
        if (test_flags(mode, file::mode::append))
            ios_mode |= ios_base::app;
    }

    if (test_flags(mode, file::mode::at_end))
        ios_mode |= ios_base::ate;

    if (handle.open(path, ios_mode) == nullptr)
        throw file_error("failed opening file");
}

file::file(file&& other) : handle(std::move(handle))
{
}

file& file::operator = (file&& other)
{
    handle = std::move(other.handle);
    return *this;
}

file::~file()
{
}

file::position_type file::position() const
{
    // The const_cast is needed because filebuf doesn't provide a const-qualified
    // method for accessing the current position.  The standard guarantees that no
    // changes will be made when calling pubseekoff with these arguments.
    return const_cast<filebuf&>(handle).pubseekoff(0, ios_base::cur);
}

void file::set_position(position_type pos)
{
    handle.pubseekpos(pos);
}

void file::seek(difference_type distance, seek_from from)
{
    ios_base::seekdir ios_from;
    if (from == seek_from::beginning)
        ios_from = ios_base::beg;
    else if (from == seek_from::current)
        ios_from = ios_base::cur;
    else if (from == seek_from::end)
        ios_from = ios_base::end;
    else
        throw file_error("invalid argument: seek_from", make_error_code(errc::invalid_argument));

    handle.pubseekoff(distance, ios_from);
}

bool file::at_end() const
{
    // Here's another lame const_cast.  This time, I'm not actually sure that the object
    // won't be mutated, as in_avail can call showmanyc, which may prompt the filebuf to
    // refresh its input buffer.  For our purposes, this is still logically const.
    return const_cast<filebuf&>(handle).in_avail() == 0;
}

file::size_type file::read(void* buffer, size_type size)
{
    return size_type(handle.sgetn(static_cast<char*>(buffer), size));
}

void file::write(const void* buffer, size_type size)
{
    if (handle.sputn(static_cast<const char*>(buffer), size) != size)
        throw file_error("error writing data to file", make_error_code(errc::io_error));
}

file_error::file_error(const std::string& msg, const std::error_code& ec) : system_error(ec, msg)
{
}

file_error::file_error(const char* msg, const std::error_code& ec) : system_error(ec, msg)
{
}
