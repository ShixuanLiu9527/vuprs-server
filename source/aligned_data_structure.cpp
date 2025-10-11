#include "aligned_data_structure.h"

/* --------------------------------------------------------------------------------------------------------------- */
/* ---------------------------------------- Aligned Data Structure ----------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::AlignedBufferDMA::AlignedBufferDMA(uint64_t byteSize)
{
    if (!this->malloc(byteSize))
    {
        throw std::bad_alloc();
    }
}

vuprs::AlignedBufferDMA::~AlignedBufferDMA()
{
    this->release();
}

void vuprs::AlignedBufferDMA::release()
{
    if (this->allocated != nullptr)
    {
#ifdef _WIN32

        _aligned_free(this->allocated);

#else
        
        free(this->allocated);

#endif
    }

    this->byteSize = 0;
    this->byteCapacity = 0;
    this->allocated = nullptr;
}

bool vuprs::AlignedBufferDMA::malloc(uint64_t byteSize)
{
    this->release();

#ifdef _WIN32

    /*
        To ensure address is aligned, size <- size + __XDMA_DMA_ALIGNMENT_BYTES__
    */
    this->allocated = _aligned_malloc(byteSize + __XDMA_DMA_ALIGNMENT_BYTES__, __XDMA_DMA_ALIGNMENT_BYTES__);

#else

    /*
        To ensure address is aligned, size <- size + __XDMA_DMA_ALIGNMENT_BYTES__
    */
    if (posix_memalign(&this->allocated, __XDMA_DMA_ALIGNMENT_BYTES__, byteSize + __XDMA_DMA_ALIGNMENT_BYTES__) != 0)
    {
        this->allocated = nullptr;
    }

#endif

    if (this->allocated == nullptr)
    {
        this->release();
        return false;
    }
    else  /* Check aligned result */
    {
        uintptr_t allocated_check = reinterpret_cast<uintptr_t>(this->allocated);
        if (allocated_check % __XDMA_DMA_ALIGNMENT_BYTES__ != 0)
        {
            this->release();
        }
    }
    this->byteSize = byteSize;
    this->byteCapacity = byteSize + __XDMA_DMA_ALIGNMENT_BYTES__;
    return true;
}

uint64_t vuprs::AlignedBufferDMA::size() const 
{ 
    return this->byteSize; 
}

uint64_t vuprs::AlignedBufferDMA::capacity() const
{
    return this->byteCapacity;
}

void* vuprs::AlignedBufferDMA::data() const 
{ 
    return this->allocated; 
}

bool vuprs::AlignedBufferDMA::is_allocated() const 
{ 
    return this->allocated != nullptr; 
}

bool vuprs::AlignedBufferDMA::to_file(const std::string &fileName, const uint64_t &fileOffset, uint64_t writeBytes) const
{
    if (!this->is_allocated() || this->byteSize == 0 || writeBytes == 0)
    {
        return false;
    }
    if (fileName.empty())
    {
        throw std::runtime_error("Empty filename.");
    }

    int file_fd = -1;
    ssize_t currentWriteBytes = 0, seekPosition = -1;
    uint64_t targetWriteBytes = std::min(this->byteSize, writeBytes);

    /* Open file */

#ifdef _WIN32

    file_fd = open(fileName.c_str(), O_RDWR | O_CREAT | O_BINARY, 0666);

#else

    file_fd = open(fileName.c_str(), O_RDWR | O_CREAT, 0666);

#endif

    if (file_fd < 0)
    {
        return false;
    }

    seekPosition = lseek(file_fd, fileOffset, SEEK_SET);

    if (static_cast<uint64_t>(seekPosition) != fileOffset || seekPosition == (off_t) - 1 || seekPosition < 0)
    {
        close(file_fd);
        return false;
    }

    currentWriteBytes = write(file_fd, this->allocated, targetWriteBytes);

    if (targetWriteBytes != static_cast<uint64_t>(currentWriteBytes))
    {
        close(file_fd);
        return false;
    }

    close(file_fd);
    return true;
}

bool vuprs::AlignedBufferDMA::from_file(const std::string &fileName, const uint64_t &fileOffset, uint64_t loadBytes)
{
    if (fileName.empty())
    {
        throw std::runtime_error("Empty filename.");
    }
    if (loadBytes == 0)
    {
        return false;
    }

    int file_fd = -1;
    ssize_t currentReadBytes = 0, seekPosition = -1;

    /* malloc */

    if(!this->malloc(loadBytes))
    {
        return false;
    }

#ifdef _WIN32

    file_fd = open(fileName.c_str(), O_RDONLY | O_BINARY);

#else

    file_fd = open(fileName.c_str(), O_RDONLY);

#endif

    if (file_fd < 0)
    {
        return false;
    }

    seekPosition = lseek(file_fd, fileOffset, SEEK_SET);

    if (static_cast<uint64_t>(seekPosition) != fileOffset || seekPosition == (off_t) - 1 || seekPosition < 0)
    {
        close(file_fd);
        return false;
    }

    currentReadBytes = read(file_fd, this->allocated, loadBytes);

    if (static_cast<uint64_t>(currentReadBytes) != loadBytes)
    {
        close(file_fd);
        return false;
    }
    
    close(file_fd);
    return true;
}
