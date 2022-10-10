#pragma once
#include "zec_types.h"


namespace zec
{
    struct Allocation
    {
        size_t byte_size = 0;
        uint8_t* ptr = nullptr;
    };
    
    class IAllocator
    {
    public:
        virtual Allocation alloc(const size_t alloc_size) = 0;
        virtual void release(Allocation& allocation) = 0;
    };

    class SimpleAllocator : public IAllocator
    {
    public:
        SimpleAllocator() = default;
        SimpleAllocator(const size_t max_capacity);
        ~SimpleAllocator();

        void initialize();
        
        // Use this if you want to re-use the memory and reset allocated_bytes to zero
        // Checks for whether we've actually released all the allocations it originally issued
        void reset();

        Allocation alloc(const size_t alloc_size) override;
        // This helps us properly track whether we're leaking these allocations accidently.
        // Doesn't really do anything except let us trip an assert on destruction/release
        void release(Allocation& allocation) override;
    
    private:
        uint8_t* ptr = nullptr;
        size_t allocated_bytes = 0;
        size_t max_capacity = 0;
        int32_t num_allocations_issued = 0;
    };
}
