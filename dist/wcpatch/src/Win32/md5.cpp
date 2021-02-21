#include "platform.h"
#include "md5.h"
#include "windows_error.h"

#include <stdext/scope_guard.h>

#include <string>

#include <WinCrypt.h>


md5_hash::md5_hash(const void* data, size_t size)
{
    HCRYPTPROV provider;
    if (!::CryptAcquireContext(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
        throw windows_error();
    at_scope_exit([&]{ ::CryptReleaseContext(provider, 0); });

    HCRYPTHASH hash;
    if (!::CryptCreateHash(provider, CALG_MD5, 0, 0, &hash))
        throw windows_error();
    at_scope_exit([&]{ ::CryptDestroyHash(hash); });

    if (!::CryptHashData(hash, static_cast<const BYTE*>(data), DWORD(size), 0))
        throw windows_error();

    DWORD hash_size;
    DWORD sz = sizeof(DWORD);
    if (!::CryptGetHashParam(hash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&hash_size), &sz, 0))
        throw windows_error();

    if (hash_size != sizeof(*this))
        throw std::logic_error("Unexpected MD5 hash size: " + std::to_string(hash_size));

    sz = sizeof(*this);
    if (!::CryptGetHashParam(hash, HP_HASHVAL, reinterpret_cast<BYTE*>(this), &sz, 0))
        throw windows_error();
}

md5_hash::md5_hash(std::initializer_list<uint32_t> elems)
{
    if (elems.size() != 4)
        throw std::invalid_argument("md5_hash must be initialized with four values");

    auto i = begin(elems);
    a = *i++;
    b = *i++;
    c = *i++;
    d = *i;
}
