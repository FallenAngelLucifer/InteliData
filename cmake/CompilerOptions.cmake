# CompilerOptions.cmake - Configuración de flags de compilación multiplataforma

function(target_apply_intelidata_compiler_flags TARGET_NAME)
    target_compile_features(${TARGET_NAME} PUBLIC cxx_std_20)

    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE
            /W4
            /permissive-
            /utf-8
            /EHsc
            /Zc:__cplusplus
            /volatile:iso
            $<$<CONFIG:Release>:/O2 /GL /Gw>
        )
        if(INTELIDATA_ENABLE_AVX2)
            target_compile_options(${TARGET_NAME} PRIVATE /arch:AVX2)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${TARGET_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wformat=2
            $<$<CONFIG:Release>:-O3>
        )
        if(INTELIDATA_ENABLE_AVX2)
            target_compile_options(${TARGET_NAME} PRIVATE -mavx2 -mfma)
        endif()
    endif()
endfunction()
