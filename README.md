# rarena

`rarena` is a small C++20 thread-local linear arena and a
standard-library-compatible allocator. The test program uses the allocator
with `std::vector`, prints a few values and statistics, then resets the arena.

## Build and run

Requirements: a C++20 compiler and GNU Make.

```sh
make
./arena_test
```

To remove the generated executable:

```sh
make clean
```

The program prints output similar to:

```text
Arena vector stats:
  size: 1000
  capacity: 1000
  first value: 0
  last value: 999
  sum: 499500
```

## How it works

- `ThreadLocalArena` lazily creates a fixed-capacity buffer for each thread that
  allocates from it. Its default capacity is 1 MiB per thread.
- `allocate` aligns each allocation and throws `std::bad_alloc` if the buffer
  does not have enough space. Individual deallocations are no-ops; resetting
  the arena reclaims all allocated space at once.
- `ArenaAllocator<T>` adapts the arena to the allocator interface expected by
  standard containers. Allocations have at least 64-byte alignment (or the
  alignment required by `T`, if greater).
- `arena_test.cpp` creates a 4 MiB-per-thread arena, uses its allocator for a
  vector of 1,000 integers, and prints the vector's size, capacity, first and
  last values, and sum. Once the vector is out of scope, `reset_all_threads()`
  reclaims the arena's storage for reuse.

The test is a short demonstration, not a benchmark. The arena must outlive the
containers that use its allocator, and memory must not be reset while those
containers still rely on it.
