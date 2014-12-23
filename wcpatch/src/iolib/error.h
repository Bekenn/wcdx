#ifndef IOLIB_ERROR_INCLUDED
#define IOLIB_ERROR_INCLUDED
#pragma once

#include <stdexcept>

namespace iolib {

/// Defines the type of objects thrown to indicate that a particular operation was invalid
/// given the current execution context.
class invalid_operation : public std::logic_error
{
public:
	explicit invalid_operation(const std::string& what_arg) : logic_error(what_arg) { }
	explicit invalid_operation(const char* what_arg)        : logic_error(what_arg) { }
};

/// Defines the type of objects thrown to indicate that a particular operation, while valid,
/// has not yet been implemented by the callee.
class not_implemented : public std::logic_error
{
public:
	explicit not_implemented(const std::string& what_arg) : logic_error(what_arg) { }
	explicit not_implemented(const char* what_arg)        : logic_error(what_arg) { }
};

} // namespace iolib

#endif
