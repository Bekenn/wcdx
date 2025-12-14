#include <image/image.h>

#include <stdext/array_view.h>
#include <stdext/multi.h>
#include <stdext/scope_guard.h>
#include <stdext/stream.h>
#include <stdext/utility.h>

#include <cassert>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
struct IUnknown;
#include <comdef.h>
#include <wincodec.h>


#define COM_REQUIRE_SUCCESS(expr) if (FAILED(hr = (expr))) _com_raise_error(hr)

namespace wcdx::image
{
    namespace
    {
        _COM_SMARTPTR_TYPEDEF(IWICImagingFactory, __uuidof(IWICImagingFactory));
        _COM_SMARTPTR_TYPEDEF(IWICBitmap, __uuidof(IWICBitmap));
        _COM_SMARTPTR_TYPEDEF(IWICPalette, __uuidof(IWICPalette));
        _COM_SMARTPTR_TYPEDEF(IWICBitmapLock, __uuidof(IWICBitmapLock));
        _COM_SMARTPTR_TYPEDEF(IWICStream, __uuidof(IWICStream));
        _COM_SMARTPTR_TYPEDEF(IWICBitmapEncoder, __uuidof(IWICBitmapEncoder));
        _COM_SMARTPTR_TYPEDEF(IWICBitmapFrameEncode, __uuidof(IWICBitmapFrameEncode));
        _COM_SMARTPTR_TYPEDEF(IWICBitmapDecoder, __uuidof(IWICBitmapDecoder));
        _COM_SMARTPTR_TYPEDEF(IWICBitmapFrameDecode, __uuidof(IWICBitmapFrameDecode));
        _COM_SMARTPTR_TYPEDEF(IWICFormatConverter, __uuidof(IWICFormatConverter));

        class OutputStreamAdapter : public IStream
        {
        public:
            OutputStreamAdapter(stdext::multi_ref<stdext::output_stream, stdext::seekable>& stream) noexcept
                : _stream(stream.as<stdext::output_stream>()), _seeker(stream.as<stdext::seekable>()) { }
            OutputStreamAdapter(const OutputStreamAdapter&) = delete;
            OutputStreamAdapter& operator = (const OutputStreamAdapter&) = delete;

        public:
            // IUnknown
            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
            ULONG STDMETHODCALLTYPE AddRef() override;
            ULONG STDMETHODCALLTYPE Release() override;

            // ISequentialStream
            HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead) override;
            HRESULT STDMETHODCALLTYPE Write(const void* pv, ULONG cb, ULONG* pcbWritten) override;

            // IStream
            HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) override;
            HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER libNewSize) override;
            HRESULT STDMETHODCALLTYPE CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) override;
            HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags) override;
            HRESULT STDMETHODCALLTYPE Revert() override;
            HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override;
            HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override;
            HRESULT STDMETHODCALLTYPE Stat(STATSTG* pstatstg, DWORD grfStatFlag) override;
            HRESULT STDMETHODCALLTYPE Clone(IStream** ppstm) override;

        private:
            stdext::output_stream& _stream;
            stdext::seekable& _seeker;
            ULONG _refcount = 1;
        };
    }

    void write_image(const image_descriptor& descriptor, stdext::array_view<const std::byte> palette_data, stdext::input_stream& pixels, stdext::multi_ref<stdext::output_stream, stdext::seekable> out)
    {
        assert(palette_data.size() == 3 * 256);  // one byte each red, green, and blue for 256 colors

        WICColor colors[256];

        auto palette_p = palette_data.data();
        auto color_p = colors;
        for (size_t n = 0; n < std::size(colors); ++n)
        {
            auto red = *palette_p++;
            auto green = *palette_p++;
            auto blue = *palette_p++;
            *color_p++ = uint8_t(blue) | (uint8_t(green) << 8) | (uint8_t(red) << 16) | (0xFF << 24);
        }
        colors[std::size(colors) - 1] &= 0x00FFFFFF; // last entry transparent

        HRESULT hr;
        COM_REQUIRE_SUCCESS(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
        at_scope_exit(::CoUninitialize);

        IWICImagingFactoryPtr imaging_factory;
        COM_REQUIRE_SUCCESS(imaging_factory.CreateInstance(CLSID_WICImagingFactory));

        IWICPalettePtr palette;
        COM_REQUIRE_SUCCESS(imaging_factory->CreatePalette(&palette));
        COM_REQUIRE_SUCCESS(palette->InitializeCustom(colors, std::size(colors)));

        IWICBitmapPtr bitmap;
        COM_REQUIRE_SUCCESS(imaging_factory->CreateBitmap(descriptor.width,
            descriptor.height,
            GUID_WICPixelFormat8bppIndexed,
            WICBitmapCacheOnDemand,
            &bitmap));
        COM_REQUIRE_SUCCESS(bitmap->SetPalette(palette));

        {
            WICRect lock_rect =
            {
                0, 0,
                INT(descriptor.width), INT(descriptor.height)
            };

            IWICBitmapLockPtr lock;
            COM_REQUIRE_SUCCESS(bitmap->Lock(&lock_rect, WICBitmapLockWrite, &lock));

            UINT buffer_size;
            WICInProcPointer bitmap_data;
            COM_REQUIRE_SUCCESS(lock->GetDataPointer(&buffer_size, &bitmap_data));

            UINT bitmap_stride;
            COM_REQUIRE_SUCCESS(lock->GetStride(&bitmap_stride));

            for (unsigned n = 0; n < descriptor.height; ++n)
            {
                pixels.read_all(bitmap_data, descriptor.width);
                bitmap_data += bitmap_stride;
            }
        }

        OutputStreamAdapter stream_adapter(out);

        IWICBitmapEncoderPtr encoder;
        COM_REQUIRE_SUCCESS(imaging_factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder));
        COM_REQUIRE_SUCCESS(encoder->Initialize(&stream_adapter, WICBitmapEncoderNoCache));

        IWICBitmapFrameEncodePtr frame_encode;
        COM_REQUIRE_SUCCESS(encoder->CreateNewFrame(&frame_encode, nullptr));
        COM_REQUIRE_SUCCESS(frame_encode->Initialize(nullptr));
        COM_REQUIRE_SUCCESS(frame_encode->WriteSource(bitmap, nullptr));
        COM_REQUIRE_SUCCESS(frame_encode->Commit());
        COM_REQUIRE_SUCCESS(encoder->Commit());
    }

    namespace
    {
        HRESULT STDMETHODCALLTYPE OutputStreamAdapter::QueryInterface(REFIID riid, void** ppvObject)
        {
            if (ppvObject == nullptr)
                return E_POINTER;

            if (IsEqualIID(riid, IID_IUnknown))
            {
                *ppvObject = static_cast<IUnknown*>(this);
                return S_OK;
            }

            if (IsEqualIID(riid, IID_ISequentialStream))
            {
                *ppvObject = static_cast<ISequentialStream*>(this);
                return S_OK;
            }

            if (IsEqualIID(riid, IID_IStream))
            {
                *ppvObject = static_cast<IStream*>(this);
                return S_OK;
            }

            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE OutputStreamAdapter::AddRef()
        {
            return ++_refcount;
        }

        ULONG STDMETHODCALLTYPE OutputStreamAdapter::Release()
        {
            return --_refcount;
        }

        HRESULT STDMETHODCALLTYPE OutputStreamAdapter::Read(void* pv, ULONG cb, ULONG* pcbRead)
        {
            return STG_E_ACCESSDENIED;
        }

        HRESULT STDMETHODCALLTYPE OutputStreamAdapter::Write(const void* pv, ULONG cb, ULONG* pcbWritten)
        {
            if (cb != 0 && pv == nullptr)
                return STG_E_INVALIDPOINTER;

            auto bytes = _stream.write(static_cast<const std::byte*>(pv), cb);
            if (pcbWritten != nullptr)
                *pcbWritten = ULONG(bytes);

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE OutputStreamAdapter::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition)
        {
            stdext::seek_from from;
            switch (dwOrigin)
            {
            case STREAM_SEEK_SET:
                from = stdext::seek_from::begin;
                break;

            case STREAM_SEEK_CUR:
                from = stdext::seek_from::current;
                break;

            case STREAM_SEEK_END:
                from = stdext::seek_from::end;
                break;

            default:
                return STG_E_INVALIDFUNCTION;
            }

            auto position = _seeker.seek(from, dlibMove.QuadPart);
            if (plibNewPosition != nullptr)
                plibNewPosition->QuadPart = position;

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE OutputStreamAdapter::SetSize(ULARGE_INTEGER libNewSize)
        {
            assert(false);
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE OutputStreamAdapter::CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten)
        {
            return STG_E_ACCESSDENIED;
        }

        HRESULT STDMETHODCALLTYPE OutputStreamAdapter::Commit(DWORD grfCommitFlags)
        {
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE OutputStreamAdapter::Revert()
        {
            return STG_E_INVALIDFUNCTION;
        }

        HRESULT STDMETHODCALLTYPE OutputStreamAdapter::LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
        {
            return STG_E_INVALIDFUNCTION;
        }

        HRESULT STDMETHODCALLTYPE OutputStreamAdapter::UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
        {
            return STG_E_INVALIDFUNCTION;
        }

        HRESULT STDMETHODCALLTYPE OutputStreamAdapter::Stat(STATSTG* pstatstg, DWORD grfStatFlag)
        {
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE OutputStreamAdapter::Clone(IStream** ppstm)
        {
            return E_NOTIMPL;
        }
    }
}
