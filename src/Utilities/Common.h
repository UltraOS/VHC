#pragma once

#include "Logger.h"
#include "Disk.h"
#include "Args.h"
#include "AutoFile.h"

#include <type_traits>

#if defined(__GNUC__) || defined(__clang__)
#define PACKED(structure) structure __attribute__((__packed__))
#elif defined(_MSC_VER)
#define PACKED(structure) __pragma(pack(push, 1)) structure __pragma(pack(pop))
#else
#error unknown compiler
#endif

template <typename T>
std::enable_if_t<std::is_integral_v<T>, T> ceiling_divide(T l, T r)
{
    return !!l + ((l - !!l) / r);
}
