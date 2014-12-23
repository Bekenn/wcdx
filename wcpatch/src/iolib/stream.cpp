#include "stream.h"

#include <algorithm>
#include <cassert>

using namespace iolib;
using namespace std;


const char seekable_base::ErrorMessage_NoSeek[] = "seeking is not supported on this stream";
const char seekable_base::ErrorMessage_PositionNotFullyImplemented[] = "create_position_handle, copy_position_handle, and set_position_from_handle must all be implemented for position methods to function";
const char seekable_base::ErrorMessage_InvalidSeek[] = "invalid attempt to seek outside the bounds of this stream";

seekable_base::position_type seekable_base::position() const
{
	return position_type(*this, create_position_handle());
}

void seekable_base::set_position(position_type pos)
{
	set_position_from_handle(pos.handle);
}

void seekable_base::seek(difference_type distance, seek_from from)
{
	if ((from == seek_from::beginning) && (distance < 0))
		throw invalid_argument(ErrorMessage_InvalidSeek);
	if ((from == seek_from::end) && (distance > 0))
		throw invalid_argument(ErrorMessage_InvalidSeek);

	do_seek(distance, from);
}


memory_input_stream::size_type memory_input_stream::do_read(void* buffer, size_type size)
{
	size = min<size_type>(size, last - current);
	copy(current, current + size, static_cast<char*>(buffer));
	current += size;
	return size;
}


void memory_output_stream::do_write(const void* buffer, size_type size)
{
	size = min<size_type>(size, last - current);
	if (size == 0)
		throw stream_write_error("illegal attempt to write to a stream that is already at the end");

	auto source = static_cast<const char*>(buffer);
	copy(source, source + size, current);
	current += size;
}

memory_stream::size_type memory_stream::do_read(void* buffer, size_type size)
{
	size = min<size_type>(size, last - current);
	copy(current, current + size, static_cast<char*>(buffer));
	current += size;
	return size;
}


void memory_stream::do_write(const void* buffer, size_type size)
{
	size = min<size_type>(size, last - current);
	if (size == 0)
		throw stream_write_error("illegal attempt to write to a stream that is already at the end");

	auto source = static_cast<const char*>(buffer);
	copy(source, source + size, current);
	current += size;
}
