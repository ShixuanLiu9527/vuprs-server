#include "fpga_io_manager.h"

vuprs::FPGA_IOManagerBase::FPGA_IOManagerBase(const std::string &deviceFilename)
{
    this->fd = -1;
    if (!this->Open(deviceFilename))
    {
        throw std::runtime_error("Cannot open device file: " + deviceFilename);
    }
}

vuprs::FPGA_IOManagerBase::FPGA_IOManagerBase()
{
    this->fd = -1;
    this->deviceFilename = "";
}

vuprs::FPGA_IOManagerBase::~FPGA_IOManagerBase()
{
    this->Close();
}

void vuprs::FPGA_IOManagerBase::Close() noexcept
{
    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

        if (this->IsOpen())
        {
            ::close(this->fd);
            this->fd = -1;
            this->deviceFilename = "";
        }
    }
}

std::string vuprs::FPGA_IOManagerBase::GetDeviceFilename() const
{
    return this->deviceFilename;
}

bool vuprs::FPGA_IOManagerBase::Open(const std::string &deviceFilename) noexcept
{
    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

        if (this->IsOpen())
        {
            if (deviceFilename == this->deviceFilename)
            {
                return true;
            }
            this->Close();
        }

        this->deviceFilename = deviceFilename; 
        this->fd = ::open(deviceFilename.c_str(), this->OpenFlags());  /* For FPGA */

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
    this->isMemoryMapped = false;
}

vuprs::FPGA_IOManagerForDevice::~FPGA_IOManagerForDevice()
{
    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
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
    if (this->IsOpen() && !this->isMemoryMapped)
    {
        this->mmap_base = ::mmap(0, __XDMA_AXI_LITE_MMAP_SIZE__, PROT_READ | PROT_WRITE, MAP_SHARED, this->fd, 0);
        if (this->mmap_base == MAP_FAILED)
        {
            throw std::runtime_error("Failed to map with mmap size = " + std::to_string(__XDMA_AXI_LITE_MMAP_SIZE__));
        }
        this->isMemoryMapped = true;
        return true;
    }
    return false;
}

bool vuprs::FPGA_IOManagerForDevice::MemoryUnmap()
{
    if (this->isMemoryMapped && this->mmap_base != MAP_FAILED)
    {
        int result = ::munmap(this->mmap_base, __XDMA_AXI_LITE_MMAP_SIZE__);
        this->isMemoryMapped = false;
        this->mmap_base = nullptr;
        return true;
    }
    return false;
}

bool vuprs::FPGA_IOManagerForDevice::RegisterIO(uint32_t* ioValue, uint32_t absoluteAddress, bool isRead)
{
    if (!this->IsOpen())
    {
        throw std::runtime_error("FPGA device file is not opened.");
    }
    if (ioValue == nullptr)
    {
        throw std::runtime_error("iovalue is NULL.");
    }

    uint8_t* _mmap_base;

    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
        _mmap_base = (uint8_t *)this->mmap_base;
    }

    try
    {
        volatile uint32_t *reg_addr = (volatile uint32_t *)(_mmap_base + absoluteAddress);
        if (isRead) 
        {
            *ioValue = ltohl(*reg_addr);
        }
        else
        {
            *reg_addr = htoll(*ioValue);
        }
    }
    catch(const std::exception& e)
    {
        throw std::runtime_error("Memory operating failed.");
    }
    return true;
}

bool vuprs::FPGA_IOManagerForDevice::RegisterListIO(std::vector<uint32_t> *ioValueList, const std::vector<uint32_t> &absoluteAddressList, bool isRead)
{
    if (!this->IsOpen())
    {
        throw std::runtime_error("FPGA device file is not opened.");
    }
    if (ioValueList == nullptr)
    {
        throw std::runtime_error("iovalue is NULL.");
    }
    if (ioValueList->size() != absoluteAddressList.size() && !isRead)
    {
        throw std::runtime_error("ioValueList.size() != absoluteAddressList.size()");
    }
    int registerNumer = absoluteAddressList.size();

    if (isRead) ioValueList->resize(registerNumer);

    uint8_t* _mmap_base;

    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
        _mmap_base = (uint8_t *)this->mmap_base;
    }

    try
    {
        for (int i = 0; i < registerNumer; i++)
        {
            volatile uint32_t *reg_addr = (volatile uint32_t *)(_mmap_base + absoluteAddressList[i]);
            if (isRead) 
            {
                (*ioValueList)[i] = ltohl(*reg_addr);
            }
            else 
            {
                *reg_addr = htoll((*ioValueList)[i]);
            }
        }
    }
    catch(const std::exception &e)
    {
        throw std::runtime_error("Memory operating failed.");
    }
    return true;
}

/* --------------------------------- FPGA IO Manager For Memories --------------------------------- */

int vuprs::FPGA_IOManagerForMemory::OpenFlags()
{
    return (O_RDWR);
}

bool vuprs::FPGA_IOManagerForMemory::OperationAfterOpened()
{
    return true;
}

bool vuprs::FPGA_IOManagerForMemory::BufferIO(void* source, uint32_t absoluteAddress, uint32_t transferBytes, bool isRead)
{
    if (!this->IsOpen())
    {
        throw std::runtime_error("FPGA device file is not opened.");
    }
    if (source == nullptr)
    {
        throw std::runtime_error("source is NULL.");
    }

    /* Seek to offset relative to AXI-Full base address in FPGA */

    int ioBytes;

    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

        off_t currentOffset = ::lseek(this->fd, absoluteAddress, SEEK_SET);
        if (static_cast<uint64_t>(currentOffset) != absoluteAddress || currentOffset < 0 || currentOffset == (off_t) - 1)
        {
            throw std::runtime_error("Seek error.");
        }
        if (isRead)
        {
            ioBytes = ::read(this->fd, source, transferBytes);
        }
        else
        {
            ioBytes = ::write(this->fd, source, transferBytes);
        }
    }
    
    return static_cast<uint64_t>(ioBytes) == transferBytes;
}

/* --------------------------------- FPGA IO Manager For Interrupt --------------------------------- */

int vuprs::FPGA_IOManagerForInterrput::OpenFlags()
{
    return (O_RDONLY);
}

bool vuprs::FPGA_IOManagerForInterrput::OperationAfterOpened()
{
    return true;
}

bool vuprs::FPGA_IOManagerForInterrput::ReadEvent(uint32_t *readValue)
{
    if (!this->IsOpen())
    {
        throw std::runtime_error("FPGA device file is not opened.");
    }

    int ioBytes;

    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
        ioBytes = ::read(this->fd, readValue, sizeof(uint32_t));
    }

     return static_cast<uint64_t>(ioBytes) == sizeof(uint32_t);
}
