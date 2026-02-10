#include "fpga_io_manager.h"

vuprs::FPGA_IOManager::FPGA_IOManager(const std::string &deviceFilename)
{
    this->fd = -1;
    this->Open(deviceFilename);
}

vuprs::FPGA_IOManager::FPGA_IOManager()
{
    this->fd = -1;
    this->deviceFilename = "";
}

vuprs::FPGA_IOManager::~FPGA_IOManager()
{
    this->Close();
}

void vuprs::FPGA_IOManager::Close() noexcept
{
    if (this->IsOpen())
    {
        ::close(this->fd);
        this->fd = -1;
        this->deviceFilename = "";
    }
}

std::string vuprs::FPGA_IOManager::GetDeviceFilename() const
{
    return this->deviceFilename;
}

bool vuprs::FPGA_IOManager::Open(const std::string &deviceFilename) noexcept
{
    if (this->IsOpen())
    {
        if (deviceFilename == this->deviceFilename)
        {
            return true;
        }
        this->Close();
    }
    
    this->deviceFilename = deviceFilename; 
    this->fd = ::open(deviceFilename.c_str(), O_RDWR | O_SYNC);  /* For FPGA */

    if (!this->IsOpen()) 
    {
        this->Close();
        return false;
    }
    return true;
}

bool vuprs::FPGA_IOManager::IsOpen() const
{
    return this->fd >= 0;
}

bool vuprs::FPGA_IOManager::BufferIO(void* source, uint32_t absoluteAddress, uint32_t transferBytes, bool isRead)
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

    off_t currentOffset = ::lseek(fd, absoluteAddress, SEEK_SET);
    if (static_cast<uint64_t>(currentOffset) != absoluteAddress || currentOffset < 0 || currentOffset == (off_t) - 1)
    {
        throw std::runtime_error("Seek error.");
    }
    int ioBytes;
    if (isRead)
    {
        ioBytes = ::read(this->fd, source, transferBytes);
    }
    else
    {
        ioBytes = ::write(this->fd, source, transferBytes);
    }
    
    return static_cast<uint64_t>(ioBytes) == transferBytes;
}

bool vuprs::FPGA_IOManager::RegisterIO(uint32_t* ioValue, uint32_t absoluteAddress, bool isRead)
{
    if (!this->IsOpen())
    {
        throw std::runtime_error("FPGA device file is not opened.");
    }
    if (ioValue == nullptr)
    {
        throw std::runtime_error("iovalue is NULL.");
    }
    void *map_base;
    
    map_base = ::mmap(0, __XDMA_AXI_LITE_MMAP_SIZE__, PROT_READ | PROT_WRITE, MAP_SHARED, this->fd, 0);
    
    if (map_base != MAP_FAILED)
    {
        /* Address convert */

        try
        {
            volatile uint32_t *reg_addr = (volatile uint32_t *)((uint8_t *)map_base + absoluteAddress);
    
            if (isRead) 
            {
                *ioValue = ltohl(*reg_addr);
            }
            else
            {
                *reg_addr = htoll(*ioValue);
            }

            ::munmap(map_base, __XDMA_AXI_LITE_MMAP_SIZE__);
            return true;
        }
        catch(const std::exception& e)
        {
            ::munmap(map_base, __XDMA_AXI_LITE_MMAP_SIZE__);
            throw std::runtime_error("Memory operating failed.");
        }
    }
    else
    {
        throw std::runtime_error("Failed to map with mmap size = " + std::to_string(__XDMA_AXI_LITE_MMAP_SIZE__));
    }
}

bool vuprs::FPGA_IOManager::RegisterListIO(std::vector<uint32_t> *ioValueList, const std::vector<uint32_t> &absoluteAddressList, bool isRead)
{
    if (!this->IsOpen())
    {
        throw std::runtime_error("FPGA device file is not opened.");
    }
    if (ioValueList->size() != absoluteAddressList.size() && !isRead)
    {
        throw std::runtime_error("ioValueList.size() != absoluteAddressList.size()");
    }
    void *map_base;
    int registerNumer = absoluteAddressList.size();

    if (isRead) ioValueList->resize(registerNumer);

    map_base = ::mmap(0, __XDMA_AXI_LITE_MMAP_SIZE__, PROT_READ | PROT_WRITE, MAP_SHARED, this->fd, 0);
    
    if (map_base != MAP_FAILED)
    {
        /* Address convert */

        try
        {
            for (int i = 0; i < registerNumer; i++)
            {
                if (ioValueList == nullptr)
                {
                    throw std::runtime_error("iovalue is NULL.");
                }
                volatile uint32_t *reg_addr = (volatile uint32_t *)((uint8_t *)map_base + absoluteAddressList[i]);
                if (isRead) 
                {
                    (*ioValueList)[i] = ltohl(*reg_addr);
                }
                else
                {
                    *reg_addr = htoll((*ioValueList)[i]);
                }
            }
            ::munmap(map_base, __XDMA_AXI_LITE_MMAP_SIZE__);
            return true;
        }
        catch(const std::exception &e)
        {
            ::munmap(map_base, __XDMA_AXI_LITE_MMAP_SIZE__);
            throw std::runtime_error("Memory operating failed.");
        }
    }
    else
    {
        throw std::runtime_error("Failed to map with mmap size = " + std::to_string(__XDMA_AXI_LITE_MMAP_SIZE__));
    }
}
