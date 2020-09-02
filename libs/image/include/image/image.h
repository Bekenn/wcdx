#include <cstddef>


namespace stdext
{
    template <class T> class array_view;
    template <class... Interfaces> class multi_ref;
    class input_stream;
    class output_stream;
    class seekable;
}

namespace wcdx::image
{
    struct image_descriptor
    {
        unsigned width;
        unsigned height;
    };

    void write_image(const image_descriptor& descriptor, stdext::array_view<const std::byte> palette, stdext::input_stream& pixels, stdext::multi_ref<stdext::output_stream, stdext::seekable> out);
}
