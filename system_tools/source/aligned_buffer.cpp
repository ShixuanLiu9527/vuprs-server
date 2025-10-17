#include "aligned_buffer.h"

/* --------------------------------------------------------------------------------------------------------------- */
/* ---------------------------------------- Aligned Data Structure ----------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::AlignedBuffer::AlignedBuffer()
{
    this->byteSize = 0;
    this->byteCapacity = 0;
    this->allocated = nullptr;
}

bool vuprs::AlignedBuffer::malloc(uint64_t byteSize)
{
    void* _allocated;
    
    this->release();  /* free all */

#ifdef _WIN32

    /*
        To ensure address is aligned, size <- size + __DEFAULT_ALIGNMENT_BYTES__
    */
    this->allocated = _aligned_malloc(byteSize + __DEFAULT_ALIGNMENT_BYTES__, __DEFAULT_ALIGNMENT_BYTES__);

#else

    /*
        To ensure address is aligned, size <- size + __DEFAULT_ALIGNMENT_BYTES__
    */
    if (posix_memalign(&_allocated, __DEFAULT_ALIGNMENT_BYTES__, byteSize + __DEFAULT_ALIGNMENT_BYTES__) != 0)
    {
        _allocated = nullptr;
    }

#endif

    if (_allocated == nullptr)
    {
        this->release();
        return false;
    }
    else  /* Check aligned result */
    {
        uintptr_t allocated_check = reinterpret_cast<uintptr_t>(_allocated);
        if (allocated_check % __DEFAULT_ALIGNMENT_BYTES__ != 0)
        {
            this->release();
            return false;
        }
    }

    this->set_bytesize(byteSize);
    this->set_capacity(byteSize + __DEFAULT_ALIGNMENT_BYTES__);
    this->set_allocated(_allocated);

    return true;
}

vuprs::AlignedBuffer::AlignedBuffer(uint64_t byteSize)
{
    if (!this->malloc(byteSize))
    {
        throw std::bad_alloc();
    }
}

vuprs::AlignedBuffer::~AlignedBuffer()
{
    this->release();
}

void vuprs::AlignedBuffer::set_bytesize(const uint64_t &bytesize)
{
    this->byteSize = bytesize;
}

void vuprs::AlignedBuffer::set_capacity(const uint64_t &capacity)
{
    this->byteCapacity = capacity;
}

void vuprs::AlignedBuffer::set_allocated(void* allocated)
{
    this->allocated = allocated;
}

void vuprs::AlignedBuffer::release()
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

uint64_t vuprs::AlignedBuffer::size() const 
{ 
    return this->byteSize; 
}

uint64_t vuprs::AlignedBuffer::capacity() const
{
    return this->byteCapacity;
}

void* vuprs::AlignedBuffer::data() const 
{ 
    return this->allocated; 
}

bool vuprs::AlignedBuffer::is_allocated() const 
{ 
    return this->allocated != nullptr; 
}

bool vuprs::AlignedBuffer::to_file(const std::string &fileName)
{
    return this->to_file(fileName, 0, this->byteSize);
}

bool vuprs::AlignedBuffer::to_file(const std::string &fileName, const uint64_t &fileOffset, uint64_t writeBytes) const
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
    
    uint64_t targetWriteBytes = (writeBytes == 0) ? this->byteSize : std::min(this->byteSize, writeBytes);

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

bool vuprs::AlignedBuffer::from_file(const std::string &fileName)
{
    struct stat file_stat;
    if (stat(fileName.c_str(), &file_stat) == -1) 
    {
        return false;
    }

    off_t fileBytesize = file_stat.st_size;

    if (fileBytesize <= 0)
    {
        return false;
    }

    return this->from_file(fileName, 0, static_cast<uint64_t>(fileBytesize));
}

bool vuprs::AlignedBuffer::from_file(const std::string &fileName, const uint64_t &fileOffset, uint64_t loadBytes)
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
        this->release();
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

bool vuprs::AlignedBufferDMA::malloc(uint64_t byteSize)
{
    void* _allocated;
    
    this->release();  /* free all */

#ifdef _WIN32

    /*
        To ensure address is aligned, size <- size + __XDMA_DMA_ALIGNMENT_BYTES__
    */
    this->allocated = _aligned_malloc(byteSize + __XDMA_DMA_ALIGNMENT_BYTES__, __XDMA_DMA_ALIGNMENT_BYTES__);

#else

    /*
        To ensure address is aligned, size <- size + __XDMA_DMA_ALIGNMENT_BYTES__
    */
    if (posix_memalign(&_allocated, __XDMA_DMA_ALIGNMENT_BYTES__, byteSize + __XDMA_DMA_ALIGNMENT_BYTES__) != 0)
    {
        _allocated = nullptr;
    }

#endif

    if (_allocated == nullptr)
    {
        this->release();
        return false;
    }
    else  /* Check aligned result */
    {
        uintptr_t allocated_check = reinterpret_cast<uintptr_t>(_allocated);
        if (allocated_check % __XDMA_DMA_ALIGNMENT_BYTES__ != 0)
        {
            this->release();
            return false;
        }
    }

    this->set_bytesize(byteSize);
    this->set_capacity(byteSize + __XDMA_DMA_ALIGNMENT_BYTES__);
    this->set_allocated(_allocated);

    return true;
}

bool vuprs::AlignedBufferServer::malloc(uint64_t byteSize)
{
    void* _allocated;
    
    this->release();  /* free all */

#ifdef _WIN32

    /*
        To ensure address is aligned, size <- size + __SERVER_ALIGNMENT_BYTES__
    */
    this->allocated = _aligned_malloc(byteSize + __SERVER_ALIGNMENT_BYTES__, __SERVER_ALIGNMENT_BYTES__);

#else

    /*
        To ensure address is aligned, size <- size + __SERVER_ALIGNMENT_BYTES__
    */
    if (posix_memalign(&_allocated, __SERVER_ALIGNMENT_BYTES__, byteSize + __SERVER_ALIGNMENT_BYTES__) != 0)
    {
        _allocated = nullptr;
    }

#endif

    if (_allocated == nullptr)
    {
        this->release();
        return false;
    }
    else  /* Check aligned result */
    {
        uintptr_t allocated_check = reinterpret_cast<uintptr_t>(_allocated);
        if (allocated_check % __SERVER_ALIGNMENT_BYTES__ != 0)
        {
            this->release();
            return false;
        }
    }

    this->set_bytesize(byteSize);
    this->set_capacity(byteSize + __SERVER_ALIGNMENT_BYTES__);
    this->set_allocated(_allocated);

    return true;
}
