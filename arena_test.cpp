
#include "include/rarena.h"
#include <iostream>
#include <numeric>

// Your global or subsystem-level Arena Coordinator
ThreadLocalArena g_frame_arena(4 * 1024 * 1024); // 4MB per thread

void process_frame_data()
{
    // Construct the allocator wrapper referencing our engine
    ArenaAllocator<int> allocator(g_frame_arena);

    // Pass the allocator directly into the vector constructor
    std::vector<int, ArenaAllocator<int>> frame_vector(allocator);

    // Operations here run with raw pointer calculation speeds
    frame_vector.reserve(1000);
    for (int i = 0; i < 1000; ++i)
    {
        frame_vector.push_back(i);
    }

    const int sum = std::accumulate(frame_vector.begin(), frame_vector.end(), 0);
    std::cout << "Arena vector stats:\n"
              << "  size: " << frame_vector.size() << '\n'
              << "  capacity: " << frame_vector.capacity() << '\n'
              << "  first value: " << frame_vector.front() << '\n'
              << "  last value: " << frame_vector.back() << '\n'
              << "  sum: " << sum << '\n';
} // frame_vector out of scope. Element destructors run, but no memory is freed yet!

int main()
{
    // Typical Engine Loop
    while (true)
    {
        // 1. Run engine tasks (can be multi-threaded)
        process_frame_data();

        // 2. Render / Present frame...

        // 3. Clear all thread memory back to 0 for the next frame
        g_frame_arena.reset_all_threads();

        break; // Breaking out for demonstration safety
    }
    return 0;
}
