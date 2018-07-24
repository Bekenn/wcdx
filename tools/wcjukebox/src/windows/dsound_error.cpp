#include "dsound_error.h"

#include <unordered_map>


namespace
{
    class dsound_error_category : public std::error_category
    {
    public:
        const char* name() const noexcept override
        {
            return "DirectSound";
        }

        std::string message(int ev) const override
        {
            static const std::unordered_map<dserr, const char*> messages =
            {
                { dserr::ok,                                "The method succeeded." },
                { dserr::out_of_memory,                     "The DirectSound subsystem could not allocate sufficient memory to complete the caller's request." },
                { dserr::no_interface,                      "The requested COM interface is not available." },
                { dserr::no_virtualization,                 "The buffer was created, but another 3D algorithm was substituted." },
                { dserr::incomplete,                        "The method succeeded, but not all the optional effects were obtained." },
                { dserr::unsupported,                       "The function called is not supported at this time." },
                { dserr::generic,                           "An undetermined error occurred inside the DirectSound subsystem." },
                { dserr::no_aggregation,                    "The object does not support aggregation." },
                { dserr::access_denied,                     "The request failed because access was denied." },
                { dserr::invalid_parameter,                 "An invalid parameter was passed to the returning function." },
                { dserr::allocated,                         "The request failed because resources, such as a priority level, were already in use by another caller." },
                { dserr::control_unavailable,               "The buffer control(volume, pan, and so on) requested by the caller is not available.Controls must be specified when the buffer is created, using the dwFlags member of DSBUFFERDESC." },
                { dserr::invalid_call,                      "This function is not valid for the current state of this object." },
                { dserr::priority_level_needed,             "A cooperative level of DSSCL_PRIORITY or higher is required." },
                { dserr::bad_format,                        "The specified wave format is not supported." },
                { dserr::no_driver,                         "No sound driver is available for use, or the given GUID is not a valid DirectSound device ID." },
                { dserr::already_initialized,               "The object is already initialized." },
                { dserr::buffer_lost,                       "The buffer memory has been lost and must be restored." },
                { dserr::other_application_has_priority,    "Another application has a higher priority level, preventing this call from succeeding." },
                { dserr::uninitialized,                     "The IDirectSound8::Initialize method has not been called or has not been called successfully before other methods were called." },
                { dserr::buffer_too_small,                  "The buffer size is not great enough to enable effects processing." },
                { dserr::ds8_required,                      "A DirectSound object of class CLSID_DirectSound8 or later is required for the requested functionality.For more information, see IDirectSound8 Interface." },
                { dserr::send_loop,                         "A circular loop of send effects was detected." },
                { dserr::bad_send_buffer_guid,              "The GUID specified in an audiopath file does not match a valid mix - in buffer." },
                { dserr::fx_unavailable,                    "The effects requested could not be found on the system, or they are in the wrong order or in the wrong location; for example, an effect expected in hardware was found in software." },
                { dserr::object_not_found,                  "The requested object was not found." }
            };

            auto i = messages.find(dserr(ev));
            if (i == messages.end())
                return "Unknown error.";
            return i->second;
        }
    };
}

const std::error_category& dsound_category() noexcept
{
    static const dsound_error_category category;
    return category;
}
