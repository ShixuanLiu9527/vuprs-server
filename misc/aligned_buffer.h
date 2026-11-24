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

#define __XDMA_DMA_ALIGNMENT_BYTES__ 4096U /* 4 kB alignment */
#define __SERVER_ALIGNMENT_BYTES__ 128U    /* 128 B alignment, must bigger than sizeof(void*) */
#define __DEFAULT_ALIGNMENT_BYTES__ 128U   /* 128 B alignment, must bigger than sizeof(void*) */

#if __XDMA_DMA_ALIGNMENT_BYTES__ < 4U
#error __XDMA_DMA_ALIGNMENT_BYTES__ must bigger than 4U.
#endif

#if __SERVER_ALIGNMENT_BYTES__ < 4U
#error __SERVER_ALIGNMENT_BYTES__ must bigger than 4U.
#endif

#if __DEFAULT_ALIGNMENT_BYTES__ < 4U
#error __DEFAULT_ALIGNMENT_BYTES__ must bigger than 4U.
#endif

namespace vuprs
{

    class AlignedBuffer
    {
    private:
        uint64_t byte_size;
        uint64_t byte_capacity;
        void *allocated;

    protected:
        void set_bytesize(const uint64_t &bytesize);
        void set_capacity(const uint64_t &capacity);
        void set_allocated(void *allocated);

    public:
        AlignedBuffer();

        explicit AlignedBuffer(uint64_t byte_size);

        virtual ~AlignedBuffer();

        /* Copy is disabled */

        AlignedBuffer(const AlignedBuffer &) = delete;
        AlignedBuffer &operator=(const AlignedBuffer &) = delete;

        /* release & malloc */

        /**
         * @brief Release buffer, pointer to nullptr, size to 0
         */
        void release();

        /**
         * @brief Aligned malloc buffer.
         *
         * @note release() will be called in this method before malloc, and do not need release in external.
         *
         * @param byte_size buffer size in bytes.
         *
         * @retval true: create success.
         * @retval false: create failed.
         *
         * @throw std::bad_loc() when error occurred.
         */
        virtual bool malloc(uint64_t byte_size);

        /**
         * @brief Indicates whether the memory has been allocated.
         *
         * @retval true, the memory has been allocated.
         * @retval false, the memory not allocated.
         */
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
        void *data() const;

        /* file IO */

        /**
         * @brief Save data from memory to file.
         *
         * @param filename target file name.
         * @param file_offset offset of file of the first data is saved.
         * @param write_bytes length of saved data.
         *
         * @retval true, save success.
         * @retval false, save failed.
         *
         * @throw std::runtime_error() when the file name is invalid.
         */
        bool to_file(const std::string &filename, const uint64_t &file_offset, uint64_t write_bytes) const;

        /**
         * @brief Save data from memory to file.
         *
         * @note Save all of the data to file.
         *
         * @param filename target file name.
         *
         * @retval true, save success.
         * @retval false, save failed.
         *
         * @throw std::runtime_error() when the file name is invalid.
         */
        bool to_file(const std::string &filename);

        /**
         * @brief Load data from file.
         *
         * @param filename target file name.
         * @param file_offset offset of file of the first data is loaded.
         * @param write_bytes length of loaded data.
         *
         * @retval true, load success.
         * @retval false, load failed.
         *
         * @throw std::runtime_error() when the file name is invalid.
         * @throw std::bad_loc() when failed to allocate.
         */
        bool from_file(const std::string &filename, const uint64_t &file_offset, uint64_t loadBytes);

        /**
         * @brief Load data from file.
         * @note Load all of the data from the certain file.
         *
         * @param filename target file name.
         *
         * @retval true, load success.
         * @retval false, load failed.
         *
         * @throw std::runtime_error() when the file name is invalid.
         * @throw std::bad_loc() when failed to allocate.
         */
        bool from_file(const std::string &filename);

        /**
         * @brief Convert buffer to vector
         *
         * @note must ensure: element_counts * sizeof(T) <= this->byte_size
         *
         * @param element_counts element counts of generated vector
         *
         * @retval vector
         *
         * @throw std::out_of_range
         */
        template <typename T>
        std::vector<T> to_vector(uint64_t element_counts) const
        {
            if (element_counts * sizeof(T) > this->byte_size)
            {
                throw std::out_of_range("in [AlignedBuffer::to_vector] Requested size exceeds buffer capacity");
            }

            T *data_ptr = reinterpret_cast<T *>(this->allocated);
            return std::vector<T>(data_ptr, data_ptr + element_counts);
        }

        /**
         * @brief Convert buffer to vector (convert all of the data)
         *
         * @retval vector
         *
         * @throw std::out_of_range
         */
        template <typename T>
        std::vector<T> to_vector() const
        {
            if (this->byte_size == 0)
            {
                throw std::out_of_range("in [AlignedBuffer::to_vector] Buffer size is 0.");
            }
            uint64_t element_counts = this->byte_size / sizeof(T);
            T *data_ptr = reinterpret_cast<T *>(this->allocated);
            return std::vector<T>(data_ptr, data_ptr + element_counts);
        }

        template <typename T>
        void from_vector(const std::vector<T> &vec)
        {
            if (vec.empty())
            {
                throw std::out_of_range("in [AlignedBuffer::from_vector] No data to convert.");
            }
            uint64_t required_bytes = vec.size() * sizeof(T);
            if (required_bytes > this->byte_size)
            {
                if (!this->malloc(required_bytes))
                {
                    throw std::bad_alloc();
                }
            }
            std::memcpy(this->allocated, vec.data(), required_bytes);
        }

        /* type transfer */

        /**
         * @brief Modify the type of the memory pointer.
         *
         * @retval Designated type of the memory pointer.
         */
        template <typename T>
        T *as() const
        {
            return reinterpret_cast<T *>(this->allocated);
        }
    };

    /**
     * @brief aligned buffer for FPGA DMA transfer
     *
     * @note aligned byte size = 4096 UL
     */
    class AlignedBufferDMA : public AlignedBuffer
    {
    public:
        AlignedBufferDMA() = default;
        explicit AlignedBufferDMA(uint64_t byte_size) : AlignedBuffer(byte_size) {}

        ~AlignedBufferDMA() override = default;

        bool malloc(uint64_t byte_size) override;
    };

    /**
     * @brief aligned buffer for TCP server
     *
     * @note aligned byte size = 4 UL
     */
    class AlignedBufferServer : public AlignedBuffer
    {
    public:
        AlignedBufferServer() = default;
        explicit AlignedBufferServer(uint64_t byte_size) : AlignedBuffer(byte_size) {}

        ~AlignedBufferServer() override = default;

        bool malloc(uint64_t byte_size) override;
    };
}

#endif
