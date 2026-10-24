function(urdm_set_warnings target)
    target_compile_options(${target} PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion
        -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Woverloaded-virtual
        -Wformat=2 -Wimplicit-fallthrough)
    target_compile_definitions(${target} PRIVATE
        QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII QT_NO_URL_CAST_FROM_STRING
        QT_NO_NARROWING_CONVERSIONS_IN_CONNECT QT_DISABLE_DEPRECATED_UP_TO=0x060800)
endfunction()

# Hardening flags for the binaries (the helper runs as root).
function(urdm_harden target)
    target_compile_options(${target} PRIVATE -fstack-protector-strong -fstack-clash-protection
        $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=3>)
    target_link_options(${target} PRIVATE -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack)
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()
