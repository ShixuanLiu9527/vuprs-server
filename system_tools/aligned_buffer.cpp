#include "system_tools/aligned_buffer.h"
#include "logger/check.h"

/* --------------------------------------------------------------------------------------------------------------- */
/* ---------------------------------------- Aligned Data Structure ----------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::AlignedBuffer::AlignedBuffer()
{
    this->byte_size = 0;
    this->byte_capacity = 0;
    this->allocated = nullptr;
}

bool vuprs::AlignedBuffer::malloc(uint64_t byte_size)
{
    void *_allocated;
    this->release(); /* free all */
#ifdef _WIN32
    /* To ensure address is aligned, size <- size + __DEFAULT_ALIGNMENT_BYTES__ */
    this->allocated = _aligned_malloc(byte_size + __DEFAULT_ALIGNMENT_BYTES__, __DEFAULT_ALIGNMENT_BYTES__);
#else
    /* To ensure address is aligned, size <- size + __DEFAULT_ALIGNMENT_BYTES__ */
    if (posix_memalign(&_allocated, __DEFAULT_ALIGNMENT_BYTES__, byte_size + __DEFAULT_ALIGNMENT_BYTES__) != 0)
    {
        _allocated = nullptr;
    }
#endif
    if (_allocated == nullptr)
    {
        this->release();
        return false;
    }
    else /* Check aligned result */
    {
        uintptr_t allocated_check = reinterpret_cast<uintptr_t>(_allocated);
        if (allocated_check % __DEFAULT_ALIGNMENT_BYTES__ != 0)
        {
            this->release();
            return false;
        }
    }
    this->set_bytesize(byte_size);
    this->set_capacity(byte_size + __DEFAULT_ALIGNMENT_BYTES__);
    this->set_allocated(_allocated);
    return true;
}

vuprs::AlignedBuffer::AlignedBuffer(uint64_t byte_size)
{
    if (!this->malloc(byte_size))
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
    this->byte_size = bytesize;
}

void vuprs::AlignedBuffer::set_capacity(const uint64_t &capacity)
{
    this->byte_capacity = capacity;
}

void vuprs::AlignedBuffer::set_allocated(void *allocated)
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
    this->byte_size = 0;
    this->byte_capacity = 0;
    this->allocated = nullptr;
}

uint64_t vuprs::AlignedBuffer::size() const
{
    return this->byte_size;
}

uint64_t vuprs::AlignedBuffer::capacity() const
{
    return this->byte_capacity;
}

void *vuprs::AlignedBuffer::data() const
{
    return this->allocated;
}

bool vuprs::AlignedBuffer::is_allocated() const
{
    return this->allocated != nullptr;
}

bool vuprs::AlignedBuffer::to_file(const std::string &filename)
{
    return this->to_file(filename, 0, this->byte_size);
}

bool vuprs::AlignedBuffer::to_file(const std::string &filename, const uint64_t &file_offset, uint64_t write_bytes) const
{
    if (!this->is_allocated() || this->byte_size == 0 || write_bytes == 0)
    {
        return false;
    }
    PARAM_CHECK(!filename.empty(), "system_tools", " in [AlignedBuffer::to_file] Empty filename.");
    int file_fd = -1;
    ssize_t current_write_bytes = 0, seek_position = -1;
    uint64_t target_write_bytes = (write_bytes == 0) ? this->byte_size : std::min(this->byte_size, write_bytes);
    /* Open file */
#ifdef _WIN32
    file_fd = open(filename.c_str(), O_RDWR | O_CREAT | O_BINARY, 0666);
#else
    file_fd = open(filename.c_str(), O_RDWR | O_CREAT, 0666);
#endif
    if (file_fd < 0)
    {
        return false;
    }
    seek_position = lseek(file_fd, file_offset, SEEK_SET);
    if (static_cast<uint64_t>(seek_position) != file_offset || seek_position == (off_t)-1 || seek_position < 0)
    {
        close(file_fd);
        return false;
    }
    current_write_bytes = write(file_fd, this->allocated, target_write_bytes);
    if (target_write_bytes != static_cast<uint64_t>(current_write_bytes))
    {
        close(file_fd);
        return false;
    }
    close(file_fd);
    return true;
}

bool vuprs::AlignedBuffer::from_file(const std::string &filename)
{
    struct stat file_stat;
    if (stat(filename.c_str(), &file_stat) == -1)
    {
        return false;
    }
    off_t file_bytesize = file_stat.st_size;
    if (file_bytesize <= 0)
    {
        return false;
    }
    return this->from_file(filename, 0, static_cast<uint64_t>(file_bytesize));
}

bool vuprs::AlignedBuffer::from_file(const std::string &filename, const uint64_t &file_offset, uint64_t loadBytes)
{
    PARAM_CHECK(!filename.empty(), "system_tools", " in [AlignedBuffer::from_file] Empty filename.");
    if (loadBytes == 0)
    {
        return false;
    }
    int file_fd = -1;
    ssize_t current_read_bytes = 0, seek_position = -1;
    /* malloc */
    if (!this->malloc(loadBytes))
    {
        this->release();
        return false;
    }
#ifdef _WIN32
    file_fd = open(filename.c_str(), O_RDONLY | O_BINARY);
#else
    file_fd = open(filename.c_str(), O_RDONLY);
#endif
    if (file_fd < 0)
    {
        return false;
    }
    seek_position = lseek(file_fd, file_offset, SEEK_SET);
    if (static_cast<uint64_t>(seek_position) != file_offset || seek_position == (off_t)-1 || seek_position < 0)
    {
        close(file_fd);
        return false;
    }
    current_read_bytes = read(file_fd, this->allocated, loadBytes);
    if (static_cast<uint64_t>(current_read_bytes) != loadBytes)
    {
        close(file_fd);
        return false;
    }
    close(file_fd);
    return true;
}

bool vuprs::AlignedBufferDMA::malloc(uint64_t byte_size)
{
    void *_allocated;
    this->release(); /* free all */
#ifdef _WIN32
    /* To ensure address is aligned, size <- size + __XDMA_DMA_ALIGNMENT_BYTES__ */
    _allocated = _aligned_malloc(byte_size + __XDMA_DMA_ALIGNMENT_BYTES__, __XDMA_DMA_ALIGNMENT_BYTES__);
#else
    /* To ensure address is aligned, size <- size + __XDMA_DMA_ALIGNMENT_BYTES__ */
    if (posix_memalign(&_allocated, __XDMA_DMA_ALIGNMENT_BYTES__, byte_size + __XDMA_DMA_ALIGNMENT_BYTES__) != 0)
    {
        _allocated = nullptr;
    }
#endif
    if (_allocated == nullptr)
    {
        this->release();
        return false;
    }
    else /* Check aligned result */
    {
        uintptr_t allocated_check = reinterpret_cast<uintptr_t>(_allocated);
        if (allocated_check % __XDMA_DMA_ALIGNMENT_BYTES__ != 0)
        {
            this->release();
            return false;
        }
    }
    this->set_bytesize(byte_size);
    this->set_capacity(byte_size + __XDMA_DMA_ALIGNMENT_BYTES__);
    this->set_allocated(_allocated);
    return true;
}

bool vuprs::AlignedBufferServer::malloc(uint64_t byte_size)
{
    void *_allocated;
    this->release(); /* free all */
#ifdef _WIN32

    /* To ensure address is aligned, size <- size + __SERVER_ALIGNMENT_BYTES__ */
    _allocated = _aligned_malloc(byte_size + __SERVER_ALIGNMENT_BYTES__, __SERVER_ALIGNMENT_BYTES__);
#else
    /* To ensure address is aligned, size <- size + __SERVER_ALIGNMENT_BYTES__ */
    if (posix_memalign(&_allocated, __SERVER_ALIGNMENT_BYTES__, byte_size + __SERVER_ALIGNMENT_BYTES__) != 0)
    {
        _allocated = nullptr;
    }
#endif
    if (_allocated == nullptr)
    {
        this->release();
        return false;
    }
    else /* Check aligned result */
    {
        uintptr_t allocated_check = reinterpret_cast<uintptr_t>(_allocated);
        if (allocated_check % __SERVER_ALIGNMENT_BYTES__ != 0)
        {
            this->release();
            return false;
        }
    }
    this->set_bytesize(byte_size);
    this->set_capacity(byte_size + __SERVER_ALIGNMENT_BYTES__);
    this->set_allocated(_allocated);
    return true;
}
