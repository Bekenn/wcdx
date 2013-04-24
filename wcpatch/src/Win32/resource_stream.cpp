#include "platform.h"

#include "../resource_stream.h"

#include <windows_error.h>
#include <windows_text.h>

#include <map>


using namespace iolib;
using namespace std;
using namespace windows;

struct resource
{
	void* data;
	size_t size;
};

struct resource_stream::impl
{
	impl(uint32_t id);
	~impl();

	uint32_t id;
	shared_ptr<resource> res;

private:
	impl(const impl&);
	impl& operator = (const impl&);

private:
	static map<uint32_t, weak_ptr<resource>> loaded_resources;
};


resource_stream::resource_stream()
{
}

resource_stream::resource_stream(uint32_t id) : pimpl(make_unique<impl>(id))
{
	*static_cast<memory_input_stream*>(this) = memory_input_stream(pimpl->res->data, pimpl->res->size);
}

resource_stream::resource_stream(resource_stream&& other) : pimpl(move(other.pimpl))
{
	*static_cast<memory_input_stream*>(this) = std::move(other);
}

resource_stream& resource_stream::operator = (resource_stream&& other)
{
	pimpl = std::move(other.pimpl);
	*static_cast<memory_input_stream*>(this) = std::move(other);
	return *this;
}

resource_stream::~resource_stream()
{
}

map<uint32_t, weak_ptr<resource>> resource_stream::impl::loaded_resources;

resource_stream::impl::impl(uint32_t id) : id(id)
{
	auto i = loaded_resources.find(id);
	if (i == loaded_resources.end() || i->second.expired())
	{
		HMODULE module;
		if (!::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, L"Nobody expects the Spanish Inquisition!", &module))
			throw windows_error();
		HRSRC hres = ::FindResource(module, MAKEINTRESOURCE(id), L"BINARY");
		if (hres == nullptr)
			throw windows_error();
		HGLOBAL hdata = ::LoadResource(module, hres);
		if (hdata == nullptr)
			throw windows_error();
		resource _res = { ::LockResource(hdata), ::SizeofResource(module, hres) };
		if ((_res.size == 0) && ::GetLastError() != ERROR_SUCCESS)
			throw windows_error();
		if (_res.data == nullptr)
			throw windows_error();
		res = make_shared<resource>(move(_res));
		loaded_resources.insert(pair<uint32_t, weak_ptr<resource>>(id, res));
	}
	else
		res = i->second.lock();
}

resource_stream::impl::~impl()
{
	res.reset();
	auto i = loaded_resources.find(id);
	if (i->second.expired())
		loaded_resources.erase(i);
}
