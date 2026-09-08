#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace b173
{
namespace render
{
// B173 - Serial values refer to ordered submissions on ONE graphics queue.
// completedSerial must come from a real fence/timeline query, never frame count.
// The platform owns submission; the renderer rejects reuse before completion.
class FrameLifetime
{
  public:
	explicit FrameLifetime(unsigned count) : slots_(count)
	{
		if (!count)
			throw std::invalid_argument("no frame slots");
	}

	void begin(unsigned slot, std::uint64_t serial, std::uint64_t completed)
	{
		if (active_ || slot >= slots_.size() || serial <= lastSerial_ || completed < completed_ || completed >= serial)
			throw std::logic_error("invalid frame submission/completion sequence");
		auto &s = slots_[slot];
		if (s.serial > completed)
			throw std::logic_error("frame slot is still in flight");
		s.references.clear();
		s.unique.clear();
		s.serial = serial;
		slot_ = slot;
		lastSerial_ = serial;
		completed_ = completed;
		active_ = true;
	}

	void end()
	{
		if (!active_)
			throw std::logic_error("no active frame");
		active_ = false;
	}

	template <class T> void retain(const std::shared_ptr<T> &p)
	{
		if (!active_)
			throw std::logic_error("resource use outside frame");
		auto &s = slots_[slot_];
		if (s.unique.insert(p.get()).second)
			s.references.push_back(p);
	}

	bool active() const
	{
		return active_;
	}

	unsigned slot() const
	{
		if (!active_)
			throw std::logic_error("no active frame");
		return slot_;
	}

	std::uint64_t serial() const
	{
		return lastSerial_;
	}

	std::uint64_t completed() const
	{
		return completed_;
	}

  private:
	struct Slot
	{
		std::uint64_t serial = 0;
		std::vector<std::shared_ptr<void>> references;
		std::unordered_set<const void *> unique;
	};

	std::vector<Slot> slots_;
	unsigned slot_ = 0;
	bool active_ = false;
	std::uint64_t lastSerial_ = 0, completed_ = 0;
};

struct ByteRange
{
	std::uint64_t offset = 0, size = 0;
};

inline std::uint64_t alignBytes(std::uint64_t value, std::uint64_t alignment)
{
	if (!alignment || (alignment & (alignment - 1)))
		throw std::invalid_argument("alignment must be a power of two");
	if (value > UINT64_MAX - (alignment - 1))
		throw std::overflow_error("aligned size overflow");
	return (value + alignment - 1) & ~(alignment - 1);
}

// GPU vertex/index arenas share this coalescing allocator. A slice is returned
// only when all in-flight shared owners retire, not when its public handle dies.
class RangeAllocator
{
  public:
	explicit RangeAllocator(std::uint64_t bytes) : size_(bytes), free_{{0, bytes}}
	{
		if (!bytes)
			throw std::invalid_argument("empty arena");
	}

	bool allocate(std::uint64_t bytes, std::uint64_t alignment, ByteRange &out)
	{
		if (!bytes)
			throw std::invalid_argument("zero allocation");
		for (std::size_t i = 0; i < free_.size(); ++i)
		{
			const auto r = free_[i];
			const auto start = alignBytes(r.offset, alignment);
			if (start - r.offset > r.size || bytes > r.size - (start - r.offset))
				continue;
			const auto end = start + bytes, oldEnd = r.offset + r.size;
			free_.erase(free_.begin() + std::ptrdiff_t(i));
			if (start > r.offset)
				free_.insert(free_.begin() + std::ptrdiff_t(i++), {r.offset, start - r.offset});
			if (end < oldEnd)
				free_.insert(free_.begin() + std::ptrdiff_t(i), {end, oldEnd - end});
			out = {start, bytes};
			return true;
		}
		return false;
	}

	void release(ByteRange r)
	{
		if (!r.size || r.offset > size_ || r.size > size_ - r.offset)
			throw std::logic_error("invalid arena release");
		auto it = std::lower_bound(free_.begin(), free_.end(), r.offset,
								   [](ByteRange a, std::uint64_t p) { return a.offset < p; });
		if ((it != free_.end() && r.offset + r.size > it->offset) ||
			(it != free_.begin() && (it - 1)->offset + (it - 1)->size > r.offset))
			throw std::logic_error("overlapping arena release");
		auto index = std::size_t(it - free_.begin());
		free_.insert(it, r);
		if (index && free_[index - 1].offset + free_[index - 1].size == free_[index].offset)
		{
			free_[index - 1].size += free_[index].size;
			free_.erase(free_.begin() + std::ptrdiff_t(index));
			--index;
		}
		if (index + 1 < free_.size() && free_[index].offset + free_[index].size == free_[index + 1].offset)
		{
			free_[index].size += free_[index + 1].size;
			free_.erase(free_.begin() + std::ptrdiff_t(index + 1));
		}
	}

	std::uint64_t freeBytes() const
	{
		std::uint64_t n = 0;
		for (auto r : free_)
			n += r.size;
		return n;
	}

  private:
	std::uint64_t size_;
	std::vector<ByteRange> free_;
};
} // namespace render
} // namespace b173
