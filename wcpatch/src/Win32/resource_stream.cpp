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


resource_stream::resource_stream(uint32_t id) : pimpl(make_unique<impl>(id))
{
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
		HGLOBAL hdata = ::LoadResource(module, hres);
		resource _res = { ::LockResource(hdata), ::SizeofResource(module, hres) };
		res = make_shared<resource>(move(_res));
		loaded_resources.insert(pair<uint32_t, weak_ptr<resource>>(id, res));
	}
	else
		res = i->second.lock();
}

resource_stream::impl::~impl()
{
}
