#ifndef PSRAM_ALLOCATOR_H
#define PSRAM_ALLOCATOR_H

// psram_allocator.h
#pragma once
#include <esp_heap_caps.h>
#include <new>            // std::bad_alloc

template <class T>
struct PSRAMAllocator {
    using value_type = T;

    T* allocate(std::size_t n)
    {
        void* p = heap_caps_malloc(n * sizeof(T),
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#if __cpp_exceptions       // compiled *with* -fexceptions?
        if (!p) throw std::bad_alloc{};
#else                      // compiled *without* exceptions
        if (!p) {
            ets_printf("PSRAM alloc failed (%zu bytes)\n", n * sizeof(T));
            abort();                    // never returns
        }
#endif
        return static_cast<T*>(p);
    }

    void deallocate(T* p, std::size_t) noexcept { heap_caps_free(p); }

    template<class U>
    bool operator==(const PSRAMAllocator<U>&) const noexcept { return true; }
    template<class U>
    bool operator!=(const PSRAMAllocator<U>&) const noexcept { return false; }
};


#endif // PSRAM_ALLOCATOR_H