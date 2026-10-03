#pragma once

#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <type_traits>

/// Tracks which other cells a single cell is connected to via springs.
///
/// One instance is stored per cell (~400k instances), so the layout is tuned
/// for memory footprint and cache behaviour rather than flexibility:
///   - Fixed capacity, no heap allocation.
///   - 16 bytes, 16-byte aligned: exactly 4 instances per 64-byte cache line,
///     and no instance ever straddles two cache lines.
///   - Trivially copyable, so it can be memcpy'd or uploaded to the GPU as-is.
///
/// Note: the order of the stored connections is NOT preserved
/// (see remove_connection).
struct alignas(16) SpringLinks
{
	/// Maximum number of springs a single cell can be attached to.
	inline static constexpr uint32_t max_connections = 3;

	/// IDs of the cells at the other end of each spring.
	/// Only the first connection_count_ entries are valid; the rest are unused.
	std::array<uint32_t, max_connections> connected_cell_ids_{};

	/// Number of valid entries in connected_cell_ids_.
	/// Deliberately 32 bits wide: the struct is padded to 16 bytes anyway, so a
	/// smaller type saves no memory and only adds widening/zero-extension work.
	uint32_t size_ = 0;

	/// Returns true if no more connections can be added.
	[[nodiscard]] constexpr bool is_full() const noexcept
	{
		return size_ >= max_connections;
	}

	/// Connects this cell to another cell.
	///
	/// Precondition: other_cell_id is not already connected (not checked, to
	/// keep this cheap) and the struct is not full. Violating the capacity
	/// precondition asserts in debug builds and is silently ignored in release.
	constexpr void add_connection(uint32_t other_cell_id) noexcept
	{
		assert(!is_full());
		if (!is_full()) [[likely]]
		{
			connected_cell_ids_[size_++] = other_cell_id;
		}
	}

	/// Removes the connection to other_cell_id, if present. Does nothing otherwise.
	///
	/// Uses swap-with-last removal: O(1) once found, but the remaining
	/// connections may be reordered. If order ever matters, shift the
	/// following elements left instead.
	constexpr void remove_connection(uint32_t other_cell_id) noexcept
	{
		for (uint32_t i = 0; i < size_; ++i)
		{
			if (connected_cell_ids_[i] == other_cell_id)
			{
				// Overwrite the removed slot with the last valid entry, then shrink.
				connected_cell_ids_[i] = connected_cell_ids_[--size_];
				return;
			}
		}
	}

	/// Removes all connections. Stale IDs are left in the array but are ignored
	/// because only the first connection_count_ entries are ever read.
	constexpr void clear_connections() noexcept
	{
		size_ = 0;
	}
};

static_assert(sizeof(SpringLinks) == 16, "Must stay at 16 bytes (4 per cache line)");
static_assert(alignof(SpringLinks) == 16, "Must be 16-byte aligned");
static_assert(std::is_trivially_copyable_v<SpringLinks>, "Must stay memcpy/GPU-upload safe");