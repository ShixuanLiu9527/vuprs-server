#include "fpga/fpga_io_manager.h"
#include "logger/log_manager.h"

vuprs::FPGA_IOManagerBase::FPGA_IOManagerBase(const std::string &device_filename)
{
    this->fd = -1;
    RUNTIME_CHECK(this->Open(device_filename), "fpga", " in [FPGA_IOManagerBase::FPGA_IOManagerBase] Cannot open device file: " + device_filename);
}

vuprs::FPGA_IOManagerBase::FPGA_IOManagerBase()
{
    this->fd = -1;
    this->device_filename = "";
}

vuprs::FPGA_IOManagerBase::~FPGA_IOManagerBase()
{
    this->Close();
}

void vuprs::FPGA_IOManagerBase::Close() noexcept
{
    {
        std::lock_guard<std::mutex> lock(this->mut); /* LOCK */

        if (this->IsOpen())
        {
            ::close(this->fd);
            this->fd = -1;
            this->device_filename = "";
        }
    }
}

std::string vuprs::FPGA_IOManagerBase::GetDeviceFilename() const
{
    return this->device_filename;
}

bool vuprs::FPGA_IOManagerBase::Open(const std::string &device_filename) noexcept
{
    {
        std::lock_guard<std::mutex> lock(this->mut); /* LOCK */
        if (this->IsOpen())
        {
            if (device_filename == this->device_filename)
            {
                return true;
            }
            this->Close();
        }
        this->device_filename = device_filename;
        this->fd = ::open(device_filename.c_str(), this->OpenFlags()); /* For FPGA */
        if (!this->IsOpen())
        {
            this->Close();
            return false;
        }
        return this->OperationAfterOpened();
    }
}

bool vuprs::FPGA_IOManagerBase::IsOpen() const
{
    return this->fd >= 0;
}

/* --------------------------------- FPGA IO Manager For Devices --------------------------------- */

vuprs::FPGA_IOManagerForDevice::FPGA_IOManagerForDevice()
{
    this->is_mmapped = false;
}

vuprs::FPGA_IOManagerForDevice::FPGA_IOManagerForDevice(const std::string &device_filename)
{
    this->is_mmapped = false;
    this->fd = -1;
    RUNTIME_CHECK(this->Open(device_filename), "fpga", " in [FPGA_IOManagerForDevice::FPGA_IOManagerForDevice] Cannot open device file: " + device_filename);
}

vuprs::FPGA_IOManagerForDevice::~FPGA_IOManagerForDevice()
{
    {
        std::lock_guard<std::mutex> lock(this->mut); /* LOCK */
        this->MemoryUnmap();
    }
}

int vuprs::FPGA_IOManagerForDevice::OpenFlags()
{
    return (O_RDWR | O_SYNC);
}

bool vuprs::FPGA_IOManagerForDevice::OperationAfterOpened()
{
    return this->MemoryMap();
}

bool vuprs::FPGA_IOManagerForDevice::MemoryMap()
{
    if (this->IsOpen() && !this->is_mmapped)
    {
        this->mmap_base = ::mmap(0, __XDMA_AXI_LITE_MMAP_SIZE__, PROT_READ | PROT_WRITE, MAP_SHARED, this->fd, 0);
        RUNTIME_CHECK(this->mmap_base != MAP_FAILED, "fpga", " in [FPGA_IOManagerForDevice::MemoryMap] Failed to map with mmap size = " + std::to_string(__XDMA_AXI_LITE_MMAP_SIZE__));
        this->is_mmapped = true;
        return true;
    }
    return false;
}

bool vuprs::FPGA_IOManagerForDevice::MemoryUnmap()
{
    if (this->is_mmapped && this->mmap_base != MAP_FAILED)
    {
        int result = ::munmap(this->mmap_base, __XDMA_AXI_LITE_MMAP_SIZE__);
        this->is_mmapped = false;
        this->mmap_base = nullptr;
        return true;
    }
    return false;
}

bool vuprs::FPGA_IOManagerForDevice::RegisterIO(uint32_t *io_value, uint32_t absolute_address, bool is_read)
{
    RUNTIME_CHECK(this->IsOpen(), "fpga", " in [FPGA_IOManagerForDevice::RegisterIO] FPGA device file is not opened.");
    PARAM_CHECK(io_value != nullptr, "fpga", " in [FPGA_IOManagerForDevice::RegisterIO] iovalue is NULL.");
    uint8_t *_mmap_base;
    {
        std::lock_guard<std::mutex> lock(this->mut); /* LOCK */
        _mmap_base = (uint8_t *)this->mmap_base;
    }
    try
    {
        volatile uint32_t *reg_addr = (volatile uint32_t *)(_mmap_base + absolute_address);
        if (is_read)
        {
            *io_value = ltohl(*reg_addr);
        }
        else
        {
            *reg_addr = htoll(*io_value);
        }
    }
    catch (const std::exception &e)
    {
        RUNTIME_CHECK(false, "fpga", " in [FPGA_IOManagerForDevice::RegisterIO] Memory operating failed.");
    }
    return true;
}

bool vuprs::FPGA_IOManagerForDevice::RegisterListIO(std::vector<uint32_t> *io_value_list, const std::vector<uint32_t> &absolute_address_list, bool is_read)
{
    RUNTIME_CHECK(this->IsOpen(), "fpga", " in [FPGA_IOManagerForDevice::RegisterListIO] FPGA device file is not opened.");
    PARAM_CHECK(io_value_list != nullptr, "fpga", " in [FPGA_IOManagerForDevice::RegisterListIO] iovalue is NULL.");
    PARAM_CHECK(is_read || io_value_list->size() == absolute_address_list.size(), "fpga", " in [FPGA_IOManagerForDevice::RegisterListIO] io_value_list.size() != absolute_address_list.size()");
    int register_numer = absolute_address_list.size();
    if (is_read)
        io_value_list->resize(register_numer);
    uint8_t *_mmap_base;
    {
        std::lock_guard<std::mutex> lock(this->mut); /* LOCK */
        _mmap_base = (uint8_t *)this->mmap_base;
    }
    try
    {
        for (int i = 0; i < register_numer; i++)
        {
            volatile uint32_t *reg_addr = (volatile uint32_t *)(_mmap_base + absolute_address_list[i]);
            if (is_read)
            {
                (*io_value_list)[i] = ltohl(*reg_addr);
            }
            else
            {
                *reg_addr = htoll((*io_value_list)[i]);
            }
        }
    }
    catch (const std::exception &e)
    {
        RUNTIME_CHECK(false, "fpga", " in [FPGA_IOManagerForDevice::RegisterListIO] Memory operating failed.");
    }
    return true;
}

/* --------------------------------- FPGA IO Manager For Memories --------------------------------- */

vuprs::FPGA_IOManagerForMemory::FPGA_IOManagerForMemory()
{
    this->fd = -1;
}

vuprs::FPGA_IOManagerForMemory::FPGA_IOManagerForMemory(const std::string &device_filename)
{
    this->fd = -1;
    RUNTIME_CHECK(this->Open(device_filename), "fpga", " in [FPGA_IOManagerForMemory::FPGA_IOManagerForMemory] Cannot open device file: " + device_filename);
}

vuprs::FPGA_IOManagerForMemory::~FPGA_IOManagerForMemory()
{
    this->fd = -1;
}

int vuprs::FPGA_IOManagerForMemory::OpenFlags()
{
    return (O_RDWR);
}

bool vuprs::FPGA_IOManagerForMemory::OperationAfterOpened()
{
    return true;
}

bool vuprs::FPGA_IOManagerForMemory::BufferIO(void *source, uint32_t absolute_address, uint32_t transferBytes, bool is_read)
{
    RUNTIME_CHECK(this->IsOpen(), "fpga", " in [FPGA_IOManagerForMemory::BufferIO] FPGA device file is not opened.");
    PARAM_CHECK(source != nullptr, "fpga", " in [FPGA_IOManagerForMemory::BufferIO] source is NULL.");
    /* Seek to offset relative to AXI-Full base address in FPGA */
    int io_bytes;
    {
        std::lock_guard<std::mutex> lock(this->mut); /* LOCK */
        off_t current_offset = ::lseek(this->fd, absolute_address, SEEK_SET);
        RUNTIME_CHECK(static_cast<uint64_t>(current_offset) == absolute_address && current_offset >= 0 && current_offset != (off_t)-1, "fpga", " in [FPGA_IOManagerForMemory::BufferIO] Seek error.");
        if (is_read)
        {
            io_bytes = ::read(this->fd, source, transferBytes);
        }
        else
        {
            io_bytes = ::write(this->fd, source, transferBytes);
        }
    }
    return static_cast<uint64_t>(io_bytes) == transferBytes;
}

/* --------------------------------- FPGA IO Manager For Interrupt --------------------------------- */

vuprs::FPGA_IOManagerForInterrput::FPGA_IOManagerForInterrput()
{
    this->fd = -1;
}

vuprs::FPGA_IOManagerForInterrput::FPGA_IOManagerForInterrput(const std::string &device_filename)
{
    this->fd = -1;
    RUNTIME_CHECK(this->Open(device_filename), "fpga", " in [FPGA_IOManagerForInterrput::FPGA_IOManagerForInterrput] Cannot open device file: " + device_filename);
}

bool vuprs::FPGA_IOManagerForInterrput::SetTimeout(uint32_t timeout_ms)
{
    if (timeout_ms < 1)
    {
        timeout_ms = 1; /* set min timeout to 1 ms */
    }
    else if (timeout_ms > 1000)
    {
        timeout_ms = 1000; /* set max timeout to 1 s */
    }

    this->timeout_ms = timeout_ms;
    return true;
}

vuprs::FPGA_IOManagerForInterrput::~FPGA_IOManagerForInterrput()
{
    this->fd = -1;
}

int vuprs::FPGA_IOManagerForInterrput::OpenFlags()
{
    return (O_RDONLY);
}

bool vuprs::FPGA_IOManagerForInterrput::OperationAfterOpened()
{
    return true;
}

bool vuprs::FPGA_IOManagerForInterrput::ReadEvent(uint32_t *read_value)
{
    RUNTIME_CHECK(this->IsOpen(), "fpga", " in [FPGA_IOManagerForInterrput::ReadEvent] FPGA device file is not opened.");
    *read_value = 0;
    fd_set read_fds;
    FD_ZERO(&read_fds);
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = this->timeout_ms * 1000; /* timeout = timeout_ms ms */
    {
        std::lock_guard<std::mutex> lock(this->mut); /* LOCK */
        FD_SET(this->fd, &read_fds);
        int select_result = ::select(this->fd + 1, &read_fds, nullptr, nullptr, &timeout); /* wait for interrupt */
        RUNTIME_CHECK(select_result >= 0, "fpga", " in [FPGA_IOManagerForInterrput::ReadEvent] Select error (return -1).");
        if (select_result == 0) /* timeout */
        {
            *read_value = 0; /* indicate no interrupt */
            return true;     /* return true for timeout */
        }
        RUNTIME_CHECK(FD_ISSET(this->fd, &read_fds), "fpga", " in [FPGA_IOManagerForInterrput::ReadEvent] No interrupt detected after select.");
        int io_bytes = ::read(this->fd, read_value, sizeof(uint32_t)); /* read interrupt event (1: interrupt detected, 0: no interrupt) */
        return static_cast<uint64_t>(io_bytes) == sizeof(uint32_t);
    }
}
