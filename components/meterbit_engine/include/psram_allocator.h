/**
 * @file psram_allocator.h
 * @brief C++ standard-library-compatible allocator that uses ESP32 PSRAM (SPIRAM).
 *
 * PSRAMAllocator<T> satisfies the Allocator named requirement and can be passed
 * as the third template argument to std::vector, std::basic_string, std::map, etc.
 * All allocations are made with MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT via heap_caps_malloc.
 *
 * When compiled without -fexceptions (the ESP-IDF default) allocation failure calls
 * abort() after printing a diagnostic. When exceptions are enabled, std::bad_alloc
 * is thrown instead.
 */
#ifndef PSRAM_ALLOCATOR_H
#define PSRAM_ALLOCATOR_H

// psram_allocator.h
#pragma once
#include <esp_heap_caps.h>
#include <new>            // std::bad_alloc

/**
 * @brief Standard-library allocator that places all allocations in ESP32 PSRAM.
 * @tparam T  Element type to allocate.
 */
template <class T>
struct PSRAMAllocator {
    using value_type = T; /**< Element type exposed to the container. */

    /**
     * @brief Allocate an array of @p n objects of type T in PSRAM.
     * @param n  Number of elements.
     * @return Pointer to the allocated block.
     * @throws std::bad_alloc if compiled with exceptions and allocation fails.
     * @note   Calls abort() on allocation failure when compiled without exceptions.
     */
    T* allocate(std::size_t n)
    {
        void* p = heap_caps_malloc(n * sizeof(T),
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#if __cpp_exceptions
        if (!p) throw std::bad_alloc{};
#else
        if (!p) {
            ets_printf("PSRAM alloc failed (%zu bytes)\n", n * sizeof(T));
            abort();
        }
#endif
        return static_cast<T*>(p);
    }

    /**
     * @brief Free a PSRAM block previously returned by allocate().
     * @param p  Pointer to the block (may be nullptr).
     */
    void deallocate(T* p, std::size_t) noexcept { heap_caps_free(p); }

    /** @brief Two PSRAMAllocators of different types are always equal (stateless). */
    template<class U>
    bool operator==(const PSRAMAllocator<U>&) const noexcept { return true; }
    /** @brief Two PSRAMAllocators of different types are never unequal (stateless). */
    template<class U>
    bool operator!=(const PSRAMAllocator<U>&) const noexcept { return false; }
};

#endif // PSRAM_ALLOCATOR_H
