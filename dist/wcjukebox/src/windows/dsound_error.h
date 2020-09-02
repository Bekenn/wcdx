#pragma once

#include <system_error>


enum class dserr
{
    ok                              = int(0x00000000),
    out_of_memory                   = int(0x00000007),
    no_interface                    = int(0x000001AE),
    no_virtualization               = int(0x0878000A),
    incomplete                      = int(0x08780014),
    unsupported                     = int(0x80004001),
    generic                         = int(0x80004005),
    no_aggregation                  = int(0x80040110),
    access_denied                   = int(0x80070005),
    invalid_parameter               = int(0x80070057),
    allocated                       = int(0x8878000A),
    control_unavailable             = int(0x8878001E),
    invalid_call                    = int(0x88780032),
    priority_level_needed           = int(0x88780046),
    bad_format                      = int(0x88780064),
    no_driver                       = int(0x88780078),
    already_initialized             = int(0x88780082),
    buffer_lost                     = int(0x88780096),
    other_application_has_priority  = int(0x887800A0),
    uninitialized                   = int(0x887800AA),
    buffer_too_small                = int(0x887810B4),
    ds8_required                    = int(0x887810BE),
    send_loop                       = int(0x887810C8),
    bad_send_buffer_guid            = int(0x887810D2),
    fx_unavailable                  = int(0x887810DC),
    object_not_found                = int(0x88781161)
};

namespace std
{
    template <> struct is_error_code_enum<dserr> : public true_type { };
}

const std::error_category& dsound_category() noexcept;
