/**
 * @brief   This document is the aligned data structure for FPGA control.
 * @version 1.0
 * @author  Shixuan Liu, Tongji University
 * @date    2025-9
 */

#ifndef ALIGNED_DATA_STRUCTURE_H
#define ALIGNED_DATA_STRUCTURE_H

#include <string>
#include <stdint.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <fcntl.h>
#include <stdexcept>
#include <cstring>

#include <assert.h>
#include <getopt.h>

#ifndef _WIN32
    #include <sys/mman.h>
#endif

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef _WIN32
#include <malloc.h>
#else
#include <cstdlib>
#endif

#define __XDMA_DMA_ALIGNMENT_BYTES__              4096U        /* 4 kB alignment */
#define __SERVER_ALIGNMENT_BYTES__                4U           /* 4 B alignment */
#define __DEFAULT_ALIGNMENT_BYTES__               4U           /* 4 B alignment */

namespace vuprs
{

    class AlignedBuffer
    {
        private:

            uint64_t byteSize;
            uint64_t byteCapacity;
            void* allocated;

        protected:

            void set_bytesize(const uint64_t &bytesize);
            void set_capacity(const uint64_t &capacity);
            void set_allocated(void* allocated);
        
        public:

            AlignedBuffer();
        
            explicit AlignedBuffer(uint64_t byteSize);

            virtual ~AlignedBuffer();

            /* Copy is disabled */
        
            AlignedBuffer(const AlignedBuffer&) = delete;
            AlignedBuffer& operator=(const AlignedBuffer&) = delete;

            /* release & malloc */
        
            /**
             * @brief Release buffer, pointer to nullptr, size to 0
             */
            void release();

            /**
             * @brief Aligned malloc buffer
             * @note release() will be called in this method before malloc, and do not need release in external.
             * @param byteSize buffer size in bytes
             * @retval true: create success
             *         false: create failed
             */
            virtual bool malloc(uint64_t byteSize);
            bool is_allocated() const;

            /* size & data* */
        
            /**
             * @brief Size in bytes.
             */
            uint64_t size() const;

            /**
             * @brief Capacity in bytes
             */
            uint64_t capacity() const;
            void* data() const;

            /* file IO */

            bool to_file(const std::string &fileName, const uint64_t &fileOffset, uint64_t writeBytes) const;
            bool to_file(const std::string &fileName);
            bool from_file(const std::string &fileName, const uint64_t &fileOffset, uint64_t loadBytes);
            bool from_file(const std::string &fileName);
        
            /**
             * @brief Convert buffer to vector
             * @note must ensure: elementCounts * sizeof(T) <= this->byteSize
             * @param elementCounts element counts of generated vector
             * @retval vector
             * @throw std::out_of_range
             */
            template<typename T>
            std::vector<T> to_vector(uint64_t elementCounts) const 
            {
                if (elementCounts * sizeof(T) > this->byteSize)
                {
                    throw std::out_of_range("Requested size exceeds buffer capacity");
                }

                T* data_ptr = reinterpret_cast<T*>(this->allocated);
                return std::vector<T>(data_ptr, data_ptr + elementCounts);
            }

            /**
             * @brief Convert buffer to vector (convert all of the data)
             * @retval vector
             * @throw std::out_of_range
             */
            template<typename T>
            std::vector<T> to_vector() const 
            {
                if (this->byteSize == 0)
                {
                    throw std::out_of_range("Buffer size is 0.");
                }

                uint64_t elementCounts = this->byteSize / sizeof(T);

                T* data_ptr = reinterpret_cast<T*>(this->allocated);
                return std::vector<T>(data_ptr, data_ptr + elementCounts);
            }
        
            template<typename T>
            void from_vector(const std::vector<T> &vec) 
            {
                if (vec.empty())
                {
                    throw std::out_of_range("No data to convert.");
                }

                uint64_t required_bytes = vec.size() * sizeof(T);
                if (required_bytes > this->byteSize) 
                {
                    if (!this->malloc(required_bytes)) 
                    {
                        throw std::bad_alloc();
                    }
                }

                std::memcpy(this->allocated, vec.data(), required_bytes);
            }

            /* type transfer */

            template <typename T>
            T* as() const
            { 
                return reinterpret_cast<T*>(this->allocated); 
            }
    };

    /**
     * @brief aligned buffer for FPGA DMA transfer
     * @note aligned byte size = 4096 UL
     */
    class AlignedBufferDMA: public AlignedBuffer
    {
        public:

            AlignedBufferDMA() = default;
            explicit AlignedBufferDMA(uint64_t byteSize) : AlignedBuffer(byteSize) {}

            ~AlignedBufferDMA() override = default;

            bool malloc(uint64_t byteSize) override;

    };

    /**
     * @brief aligned buffer for TCP server
     * @note aligned byte size = 4 UL
     */
    class AlignedBufferServer: public AlignedBuffer
    {
        public:
        
            AlignedBufferServer() = default;
            explicit AlignedBufferServer(uint64_t byteSize) : AlignedBuffer(byteSize) {}
            
            ~AlignedBufferServer() override = default;

            bool malloc(uint64_t byteSize) override;

    };
}

#endif
