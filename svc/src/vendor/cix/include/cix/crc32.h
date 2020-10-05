// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

namespace cix {
namespace crc32 {

typedef std::uint32_t hash_t;

hash_t create() noexcept;
void update(hash_t& context, const void* begin, const void* end) noexcept;
void update(hash_t& context, const void* begin, std::size_t size) noexcept;
hash_t finalize(const hash_t& ctx) noexcept;

hash_t crc32(const void* begin, const void* end) noexcept;
hash_t crc32(const void* data, std::size_t size) noexcept;

}  // namespace crc32
}  // namespace cix
