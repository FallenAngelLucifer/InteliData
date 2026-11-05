#pragma once

#include <cstdint>
#include <cstddef>
#include <bit>

// Detección de plataforma
#if defined(_WIN32) || defined(_WIN64)
    #define INTELIDATA_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define INTELIDATA_PLATFORM_LINUX 1
#elif defined(__APPLE__)
    #define INTELIDATA_PLATFORM_MACOS 1
#else
    #define INTELIDATA_PLATFORM_UNKNOWN 1
#endif

// Atributos de compilador e inlining
#if defined(_MSC_VER)
    #define INTELIDATA_FORCE_INLINE __forceinline
    #define INTELIDATA_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    #define INTELIDATA_FORCE_INLINE inline __attribute__((always_inline))
    #define INTELIDATA_NOINLINE __attribute__((noinline))
#else
    #define INTELIDATA_FORCE_INLINE inline
    #define INTELIDATA_NOINLINE
#endif

// Verificación de arquitectura de 64 bits en tiempo de compilación
static_assert(sizeof(void*) == 8, "InteliData requiere arquitectura de 64 bits (x86_64 o ARM64)");
