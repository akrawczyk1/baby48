#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace baby48::memory{
    inline constexpr std::size_t storeSize = 32;
    using MemoryImage = std::array<std::uint32_t, storeSize>;
}
