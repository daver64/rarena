#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <vector>

// ============================================================================
// 1. THE THREAD-LOCAL ARENA ENGINE
// ============================================================================
class ThreadLocalArena
{
  private:
    struct AlignmentBuffer
    {
        std::unique_ptr<std::byte[]> storage;
        std::size_t capacity;
        std::size_t offset = 0;

        AlignmentBuffer(std::size_t cap) : storage(std::make_unique<std::byte[]>(cap)), capacity(cap)
        {
        }
        void reset()
        {
            offset = 0;
        }
    };

  public:
    explicit ThreadLocalArena(std::size_t thread_capacity = 1024 * 1024) : m_default_capacity(thread_capacity)
    {
    }

    ~ThreadLocalArena() = default;

    void *allocate(std::size_t size, std::size_t alignment)
    {
        assert(std::has_single_bit(alignment));

        AlignmentBuffer &thread_buf = get_or_create_thread_buffer();

        // 1. Calculate the current raw pointer position
        uintptr_t current_ptr = reinterpret_cast<uintptr_t>(thread_buf.storage.get() + thread_buf.offset);

        // 2. Round up the pointer itself to the requested alignment
        uintptr_t aligned_ptr = (current_ptr + alignment - 1) & ~(alignment - 1);

        // 3. Compute the new offset based on the requested size
        std::size_t new_offset = aligned_ptr - reinterpret_cast<uintptr_t>(thread_buf.storage.get());
        new_offset += size;

        // 4. Pad the trailing offset to 64 bytes to ensure the *next* allocation
        // lands cleanly on a cache-line boundary.
        new_offset = (new_offset + 63) & ~63;

        if (new_offset > thread_buf.capacity)
        {
            throw std::bad_alloc(); // Standard library containers expect bad_alloc on failure
        }

        thread_buf.offset = new_offset;
        return reinterpret_cast<void *>(aligned_ptr);
    }

    // Standard containers might call deallocate, which we intentionally treat as a no-op
    void deallocate(void *, std::size_t) noexcept
    {
        // Linear arenas do not free individual allocations.
        // Memory is reclaimed entirely via reset_all_threads().
    }

    void reset_all_threads()
    {
        std::lock_guard<std::mutex> lock(m_registry_mutex);
        for (auto *buffer : m_all_buffers)
        {
            if (buffer)
            {
                buffer->reset();
            }
        }
    }

  private:
    AlignmentBuffer &get_or_create_thread_buffer()
    {
        static thread_local std::unique_ptr<AlignmentBuffer> local_buffer = nullptr;

        if (!local_buffer)
        {
            local_buffer = std::make_unique<AlignmentBuffer>(m_default_capacity);

            std::lock_guard<std::mutex> lock(m_registry_mutex);
            m_all_buffers.push_back(local_buffer.get());
        }

        return *local_buffer;
    }

    std::size_t m_default_capacity;
    std::mutex m_registry_mutex;
    std::vector<AlignmentBuffer *> m_all_buffers;
};

// ============================================================================
// 2. THE C++ STANDARD COMPLIANT WRAPPER
// ============================================================================
template <typename T> class ArenaAllocator
{
  public:
    using value_type = T;

    // Direct constructor matching our arena instance
    explicit ArenaAllocator(ThreadLocalArena &arena) noexcept : m_arena(&arena)
    {
    }

    // Boilerplate converting constructor required by standard containers
    template <typename U> constexpr ArenaAllocator(const ArenaAllocator<U> &other) noexcept : m_arena(other.m_arena)
    {
    }

    [[nodiscard]] T *allocate(std::size_t n)
    {
        // Enforce a strict minimum alignment of 64 bytes for cache line isolation,
        // but scale up if the type requires more (e.g. specialized SIMD types)
        constexpr std::size_t alignment = (alignof(T) > 64) ? alignof(T) : 64;

        return static_cast<T *>(m_arena->allocate(n * sizeof(T), alignment));
    }

    void deallocate(T *p, std::size_t n) noexcept
    {
        m_arena->deallocate(p, n * sizeof(T));
    }

    // Comparison operators tell containers if allocators can swap/deallocate each other's memory
    template <typename U> bool operator==(const ArenaAllocator<U> &other) const noexcept
    {
        return m_arena == other.m_arena;
    }

    template <typename U> bool operator!=(const ArenaAllocator<U> &other) const noexcept
    {
        return m_arena != other.m_arena;
    }

    // Keeping this public allows the converting constructor above to access the underlying pointer
    ThreadLocalArena *m_arena;
};
