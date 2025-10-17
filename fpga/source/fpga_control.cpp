#include "fpga_control.h"

/* --------------------------------------------------------------------------------------------------------------- */
/* --------------------------------------------- FPGA Controller ------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

vuprs::FPGAController::FPGAController()
{
    
}

vuprs::FPGAController::FPGAController(const std::string &configJsonFilename)
{
    this->fpgaConfigManager.LoadFPGAConfigFromJson(configJsonFilename);
}

vuprs::FPGAController::~FPGAController()
{

}

void vuprs::FPGAController::Info(const std::string &info)
{
    if (this->fpgaControllerLogger) this->fpgaControllerLogger->info(info);
}

void vuprs::FPGAController::Warn(const std::string &warn)
{
    if (this->fpgaControllerLogger) this->fpgaControllerLogger->warn(warn);
}

void vuprs::FPGAController::Error(const std::string &err)
{
    if (this->fpgaControllerLogger) this->fpgaControllerLogger->error(err);
}

void vuprs::FPGAController::Critical(const std::string &critical)
{
    if (this->fpgaControllerLogger) this->fpgaControllerLogger->critical(critical);
}

void vuprs::FPGAController::InitLogger(const std::string &loggerName, const std::string &loggerFilename)
{
    this->fpgaControllerLogger = vuprs::LogManager::getLogger(loggerName, loggerFilename);
    this->Info("FPGA controller logger started.");
}

bool vuprs::FPGAController::LoadFPGAConfig(const vuprs::FPGAConfigManager &newFPGAConfig)
{
    if (newFPGAConfig.ConfigDown())
    {
        this->fpgaConfigManager = newFPGAConfig;
        return true;
    }

    return false;
}

/* ------------------------------------------- Read/Write to value ----------------------------------------------- */

uint64_t vuprs::FPGAController::AXILite_GetRegisterOffset(const int &registerSelection, bool *status)
{
    if (status != nullptr)
    {
        *status = false;
    }
    if (!IS_AXI_LITE_REGISTER(registerSelection) || !this->fpgaConfigManager.ConfigDown())
    {
        return 0;
    }

    uint64_t axiLiteRegisterSpaceBaseAddress = 0, registerOffset = 0;

    /* Calculate base address of the register address space (relative to AXI-Lite base address) */

    if (IS_AXI_LITE_REGISTER__ADC(registerSelection))
    {
        axiLiteRegisterSpaceBaseAddress = this->fpgaConfigManager.fpgaConfig.fpgaAddress.busAddress.addrBusBaseAXILite__ADC;
    }
    else if (IS_AXI_LITE_REGISTER__DMA(registerSelection))
    {
        axiLiteRegisterSpaceBaseAddress = this->fpgaConfigManager.fpgaConfig.fpgaAddress.busAddress.addrBusBaseAXILite__DMA;
    }
    else
    {
        return 0;
    }

    /* Calculate register address (relative to axiLiteRegisterSpaceBaseAddress) */

    switch (registerSelection)
    {
        /* ADC Controller Registers */

        case AXI_LITE_REGISTER__ADC__SCI: 
        {
            registerOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.registerAddressADC.addrRegisterBaseADC__SCI;
            break;
        }
        case AXI_LITE_REGISTER__ADC__SP:
        {
            registerOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.registerAddressADC.addrRegisterBaseADC__SP;
            break;
        }
        case AXI_LITE_REGISTER__ADC__SF:
        {
            registerOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.registerAddressADC.addrRegisterBaseADC__SF;
            break;
        }
        case AXI_LITE_REGISTER__ADC__STR:
        {
            registerOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.registerAddressADC.addrRegisterBaseADC__STR;
            break;
        }
        case AXI_LITE_REGISTER__ADC__NGF:
        {
            registerOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.registerAddressADC.addrRegisterBaseADC__NGF;
            break;
        }
        case AXI_LITE_REGISTER__ADC__ERR:
        {
            registerOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.registerAddressADC.addrRegisterBaseADC__ERR;
            break;
        }

        /* DMA Controller Registers */

        case AXI_LITE_REGISTER__DMA__S2MM_DMACR:
        {
            registerOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_DMACR;
            break;
        }
        case AXI_LITE_REGISTER__DMA__S2MM_DMASR:
        {
            registerOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_DMASR;
            break;
        }
        case AXI_LITE_REGISTER__DMA__SG_CTL:
        {
            registerOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__SG_CTL;
            break;
        }
        case AXI_LITE_REGISTER__DMA__S2MM_CURDESC:
        {
            registerOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_CURDESC;
            break;
        }
        case AXI_LITE_REGISTER__DMA__S2MM_CURDESC_MSB:
        {
            registerOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_CURDESC_MSB;
            break;
        }
        case AXI_LITE_REGISTER__DMA__S2MM_TAILDESC:
        {
            registerOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_TAILDESC;
            break;
        }
        case AXI_LITE_REGISTER__DMA__S2MM_TAILDESC_MSB:
        {
            registerOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_TAILDESC_MSB;
            break;
        }
        case AXI_LITE_REGISTER__DMA__S2MM_DA:
        {
            registerOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_DA;
            break;
        }
        case AXI_LITE_REGISTER__DMA__S2MM_DA_MSB:
        {
            registerOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_DA_MSB;
            break;
        }
        case AXI_LITE_REGISTER__DMA__S2MM_LENGTH:
        {
            registerOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_LENGTH;
            break;
        }

        default: 
        {
            return 0;
        }
    }

    if (status != nullptr)
    {
        *status = true;
    }

    return axiLiteRegisterSpaceBaseAddress + registerOffset;  /* Base Address (Relative to AXI-Lite base address) + Register Offset */
}

bool vuprs::FPGAController::AXI_XDMA_WordIO(const vuprs::DMATransferConfig &transferConfig, const uint32_t &w_value, uint32_t *r_value)
{
    /* ------------------------ Security Check Start ------------------------- */

    if (!this->fpgaConfigManager.ConfigDown())
    {
        throw std::runtime_error("Config not complete.");
    }

    /* ------------------------- Security Check End -------------------------- */

    int fpga_fd = -1, writeReadStatus = -1;
    bool registerCalculateStatus = false, use_mmap = false;
    uint64_t registerTargetOffset = 0, base = 0, offset = 0;
    off_t currentOffset = -1;

    uint64_t memoryLowerAddress;
    uint64_t memoryUpperAddress;

    std::string deviceFile;

    /* Base address & accessble address */

    offset = transferConfig.offset;

    switch (transferConfig.transferMemorySelection)
    {
        case DMA_TRANSFER_MEMORY_SELECTION__DDR:
        {
            memoryLowerAddress = 0;
            memoryUpperAddress = this->fpgaConfigManager.fpgaConfig.hardwareConfig.hardwareConfigMemory.ddrMemoryCapacity_bytes - 1;
            
            base = 0;
            
            use_mmap = false;
            if (transferConfig.transferDirectionSelection == DMA_TRANSFER_DIRECTION__FPGA_TO_HOST)
            {
                deviceFile = this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_c2h[transferConfig.dmaChannel];
            }
            else
            {
                deviceFile = this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_h2c[transferConfig.dmaChannel];
            }
            break;
        }
        case DMA_TRANSFER_MEMORY_SELECTION__BRAM:
        {
            memoryLowerAddress = 0;
            memoryUpperAddress = this->fpgaConfigManager.fpgaConfig.hardwareConfig.hardwareConfigMemory.bramMemoryCapacity_bytes - 1;
            
            base = 0;
            
            use_mmap = false;
            if (transferConfig.transferDirectionSelection == DMA_TRANSFER_DIRECTION__FPGA_TO_HOST)
            {
                deviceFile = this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_c2h[transferConfig.dmaChannel];
            }
            else
            {
                deviceFile = this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_h2c[transferConfig.dmaChannel];
            }
            break;
        }
        case DMA_TRANSFER_MEMORY_SELECTION__AXI_LITE_DOMAIN:
        {
            memoryLowerAddress = this->fpgaConfigManager.fpgaConfig.fpgaAddress.busAddress.addrBusBaseAXILite__ADC;
            memoryUpperAddress = memoryLowerAddress + __XDMA_AXI_LITE_MMAP_SIZE__ - 1;

            base = transferConfig.base;
            
            deviceFile = this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_user;
            use_mmap = true;
            break;
        }
        case DMA_TRANSFER_MEMORY_SELECTION__XDMA_DOMAIN:
        {
            memoryLowerAddress = 0;
            memoryUpperAddress = memoryLowerAddress + __XDMA_CONTROL_MMAP_SIZE__ - 1;

            base = 0;
            
            deviceFile = this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_control;
            use_mmap = true;
            break;
        }
        default: 
        {
            throw std::runtime_error("Invalid memory selection.");
        }
    }

    /* Calculate register address */

    registerTargetOffset = base + offset;

    if (registerTargetOffset > (memoryUpperAddress - 3) || registerTargetOffset < memoryLowerAddress)
    {
        char buffer[256];
        sprintf(buffer, "Invalid offset for 4 bytes transfer. (valid offset: 0x%X - 0x%X)", memoryLowerAddress, memoryUpperAddress - 3);
        throw std::range_error(std::string(buffer));
    }

    /* Open device file */

#ifdef _WIN32

    fpga_fd = open(deviceFile.c_str(), O_RDWR | O_BINARY);

#else

    fpga_fd = open(deviceFile.c_str(), O_RDWR | O_SYNC);

#endif

    /* Check if device file is open */

    if (fpga_fd < 0)
    {
        throw std::runtime_error("Cannot open device file: " + deviceFile);
    }

    if (!use_mmap)
    {

        /* Seek to offset relative to AXI-Lite/AXI-Full base address in FPGA */

        currentOffset = lseek(fpga_fd, registerTargetOffset, SEEK_SET);

        if (currentOffset == (off_t) - 1)
        {
            close(fpga_fd);  /* close file */
            throw std::runtime_error("Seek error.");
        }

        /* Write data to register */

        if (transferConfig.transferDirectionSelection == DMA_TRANSFER_DIRECTION__HOST_TO_FPGA)
        {
            writeReadStatus = write(fpga_fd, &w_value, sizeof(uint32_t));  /* All registers are 32 bit */
        }
        else if (transferConfig.transferDirectionSelection == DMA_TRANSFER_DIRECTION__FPGA_TO_HOST)
        {
            if (r_value != nullptr)
            {
                writeReadStatus = read(fpga_fd, r_value, sizeof(uint32_t));  /* All registers are 32 bit */
            }
        }
    }
    else
    {
    
#ifndef _WIN32
    
        /* Generate Memory Map */

        void *map_base;
        size_t mmapSize = 1024;

        /* Calculate mmap size */

        if (deviceFile == this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_control)
        {
            mmapSize = __XDMA_CONTROL_MMAP_SIZE__;
        }
        else if (deviceFile == this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_user)
        {
            mmapSize = __XDMA_AXI_LITE_MMAP_SIZE__;
        }

        map_base = mmap(0, mmapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fpga_fd, 0);

        if (map_base != MAP_FAILED)
        {
            /* Address convert */

            volatile uint32_t *reg_addr = (volatile uint32_t *)((uint8_t *)map_base + registerTargetOffset);

            if (transferConfig.transferDirectionSelection == DMA_TRANSFER_DIRECTION__FPGA_TO_HOST) 
            {
                *r_value = ltohl(*reg_addr);
            }
            else 
            {
                *reg_addr = htoll(w_value);
            }
            
            munmap(map_base, mmapSize);
           
            writeReadStatus = sizeof(uint32_t);
        }
        else
        {
            writeReadStatus = -1;
            throw std::runtime_error("Failed to mmap: " + deviceFile + ", with mmap size = " + std::to_string(mmapSize));
        }

#endif

    }

    /* Close */

    close(fpga_fd);

    return writeReadStatus == sizeof(uint32_t);
}

bool vuprs::FPGAController::AXIFull_BufferIO(const vuprs::DMATransferConfig &transferConfig, vuprs::AlignedBufferDMA *buffer)
{
    /* ------------------------ Security Check Start ------------------------- */

    if (!this->fpgaConfigManager.ConfigDown())  /* detect at first */
    {
        throw std::runtime_error("Config not complete.");
    }

    uint64_t memoryLowerAddress;
    uint64_t memoryUpperAddress;

    if (transferConfig.transferMemorySelection == DMA_TRANSFER_MEMORY_SELECTION__DDR)
    {
        memoryLowerAddress = this->fpgaConfigManager.fpgaConfig.fpgaAddress.busAddress.addrBusBaseAXIFull__DDR;
        memoryUpperAddress = memoryLowerAddress + this->fpgaConfigManager.fpgaConfig.hardwareConfig.hardwareConfigMemory.ddrMemoryCapacity_bytes - 1;
    }
    else if (transferConfig.transferMemorySelection == DMA_TRANSFER_MEMORY_SELECTION__BRAM)
    {
        memoryLowerAddress = this->fpgaConfigManager.fpgaConfig.fpgaAddress.busAddress.addrBusBaseAXIFull__BRAM;
        memoryUpperAddress = memoryLowerAddress + this->fpgaConfigManager.fpgaConfig.hardwareConfig.hardwareConfigMemory.bramMemoryCapacity_bytes - 1;
    }
    else
    {
        throw std::runtime_error("Invalid memory selection.");
    }

    if (!IS_DMA_TRANSFER_DIRECTION(transferConfig.transferDirectionSelection))
    {
        throw std::runtime_error("Invalid direction.");
    }

    if (transferConfig.transferByteSize == 0)
    {
        throw std::runtime_error("Read bytes is 0.");
    }

    if (transferConfig.offset < memoryLowerAddress || transferConfig.offset > memoryUpperAddress)
    {
        throw std::range_error("Invalid offset. (valid offset: " + std::to_string(memoryLowerAddress) + " - " + std::to_string(memoryUpperAddress) + ")");
    }

    if ((transferConfig.offset + transferConfig.transferByteSize - 1) > memoryUpperAddress)
    {
        throw std::runtime_error("Read Domain of the DDR overflow. (valid transfer bytes of this offset = " + std::to_string(memoryUpperAddress - transferConfig.offset + 1));
    }

    if (buffer == nullptr)
    {
        throw std::runtime_error("*Buffer is nullptr.");
    }

    /* ------------------------- Security Check End -------------------------- */

    int fpga_fd = -1, writeReadStatus = -1, writeReadBytes = 0;
    uint64_t componentOffset = 0;
    ssize_t currentOffset = -1;
    
    /* Open device file (AXI-Full DMA) */

    if (transferConfig.transferDirectionSelection == DMA_TRANSFER_DIRECTION__FPGA_TO_HOST)
    {
        if (transferConfig.dmaChannel >= this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_c2h.size())
        {
            throw std::runtime_error(
                "Invalid DMA channel (required: < " + \
                std::to_string(this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_c2h.size()) + "), current = " + \
                std::to_string(transferConfig.dmaChannel)
            );
        }

        /* Open */

#ifdef _WIN32

        fpga_fd = open((this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_c2h[transferConfig.dmaChannel]).c_str(), O_RDWR | O_BINARY);

#else

        fpga_fd = open((this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_c2h[transferConfig.dmaChannel]).c_str(), O_RDWR | O_SYNC);

#endif

        /* Check if device file & input/output file is open */

        if (fpga_fd < 0)
        {
            throw std::runtime_error("Cannot open device file: " + this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_c2h[transferConfig.dmaChannel]);
        }
    }

    else if (transferConfig.transferDirectionSelection == DMA_TRANSFER_DIRECTION__HOST_TO_FPGA)
    {
        if (transferConfig.dmaChannel >= this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_h2c.size())
        {
            throw std::runtime_error(
                "Invalid DMA channel (required: < " + \
                std::to_string(this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_h2c.size()) + "), current = " + \
                std::to_string(transferConfig.dmaChannel)
            );
        }

#ifdef _WIN32

        fpga_fd = open((this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_h2c[transferConfig.dmaChannel]).c_str(), O_RDWR | O_BINARY);
    
#else

        fpga_fd = open((this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_h2c[transferConfig.dmaChannel]).c_str(), O_RDWR | O_SYNC);

#endif

        /* Check if device file & input/output file is open */

        if (fpga_fd < 0)
        {
            throw std::runtime_error("Cannot open device file: " + this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_h2c[transferConfig.dmaChannel]);
        }
    }

    /* --- Seek --- */

    /* Seek to offset relative to AXI-Full base address in FPGA */

    componentOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.busAddress.addrBusBaseAXIFull__DDR + transferConfig.offset;
    currentOffset = lseek(fpga_fd, componentOffset, SEEK_SET);

    if (static_cast<uint64_t>(currentOffset) != componentOffset || currentOffset < 0 || currentOffset == (off_t) - 1)
    {
        close(fpga_fd);
        throw std::runtime_error("Seek error.");
    }

    /* --- Read --- */

    /* Read FPGA data to buffer (READ mode) */

    if (transferConfig.transferDirectionSelection == DMA_TRANSFER_DIRECTION__FPGA_TO_HOST)
    {
        if (!buffer->malloc(transferConfig.transferByteSize))
        {
            close(fpga_fd);
            throw std::runtime_error("Cannot malloc buffer.");
        }

        writeReadBytes = read(fpga_fd, buffer->data(), transferConfig.transferByteSize);

        if (static_cast<uint64_t>(writeReadBytes) != transferConfig.transferByteSize)
        {
            close(fpga_fd);
            return false;
        }
    }

    /* --- Write --- */

    /* Write buffer data to FPGA (WRITE mode) */

    else if (transferConfig.transferDirectionSelection == DMA_TRANSFER_DIRECTION__HOST_TO_FPGA)
    {
        if (!buffer->is_allocated())
        {
            close(fpga_fd);
            throw std::runtime_error("Buffer not allocated.");
        }

        writeReadBytes = write(fpga_fd, buffer->data(), transferConfig.transferByteSize);

        if (static_cast<uint64_t>(writeReadBytes) != transferConfig.transferByteSize)
        {
            close(fpga_fd);
            return false;
        }
    }

    /* Free all */

    close(fpga_fd);

    return true;
}

/* --------------------------------------------------------------------------------------------------------------- */
/* ---------------------------------------------- User Interface ------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

/* --------------------------------------------------- AXI-Lite -------------------------------------------------- */

bool vuprs::FPGAController::AXILite_WriteToFPGARegister(const int &registerSelection, const uint32_t &w_value)
{
    uint64_t writeOffset;
    bool offsetStatus = false;
    vuprs::DMATransferConfig transferConfig;
    
    if (!IS_AXI_LITE_RDONLY_REGISTER(registerSelection) && IS_AXI_LITE_REGISTER(registerSelection) && this->fpgaConfigManager.ConfigDown())
    {
        writeOffset = this->AXILite_GetRegisterOffset(registerSelection, &offsetStatus);

        if (offsetStatus)
        {
            vuprs::SetDMATransferConfigToDefault(&transferConfig);

            transferConfig.transferDirectionSelection = DMA_TRANSFER_DIRECTION__HOST_TO_FPGA;
            transferConfig.transferMemorySelection = DMA_TRANSFER_MEMORY_SELECTION__AXI_LITE_DOMAIN;

            transferConfig.base = 0;
            transferConfig.offset = writeOffset;

            transferConfig.transferByteSize = 0;
            transferConfig.dmaChannel = 0;
        }
        else
        {
            return false;
        }

        return this->AXI_XDMA_WordIO(transferConfig, w_value, 0);
    }
    else
    {
        throw std::runtime_error("Register is read only: " + std::to_string(registerSelection));
    }
}

bool vuprs::FPGAController::AXILite_ReadFPGARegister(const int &registerSelection, uint32_t *r_value)
{
    uint64_t readOffset;
    bool offsetStatus = false;
    vuprs::DMATransferConfig transferConfig;

    if (IS_AXI_LITE_REGISTER(registerSelection) && this->fpgaConfigManager.ConfigDown())
    {
        readOffset = this->AXILite_GetRegisterOffset(registerSelection, &offsetStatus);

        if (offsetStatus)
        {
            vuprs::SetDMATransferConfigToDefault(&transferConfig);

            transferConfig.transferDirectionSelection = DMA_TRANSFER_DIRECTION__FPGA_TO_HOST;
            transferConfig.transferMemorySelection = DMA_TRANSFER_MEMORY_SELECTION__AXI_LITE_DOMAIN;

            transferConfig.base = 0;
            transferConfig.offset = readOffset;

            transferConfig.transferByteSize = 0;
            transferConfig.dmaChannel = 0;
        }
        else
        {
            return false;
        }

        return this->AXI_XDMA_WordIO(transferConfig, 0, r_value);
    }
    else
    {
        throw std::runtime_error("Register is read only: " + std::to_string(registerSelection));
    }
}

/* -------------------------------------------- AXI-Full Buffer IO ----------------------------------------------- */

bool vuprs::FPGAController::AXIFull_BufferTransfer(const vuprs::DMATransferConfig &transferConfig, vuprs::AlignedBufferDMA *buffer)
{
    if (!IS_DMA_BUFFER_TRANSFER_MEMORY_SELECTION(transferConfig.transferMemorySelection))
    {
        throw std::runtime_error("Invalid BUFFER memory selection.");
    }
    if (!IS_DMA_TRANSFER_DIRECTION(transferConfig.transferDirectionSelection))
    {
        throw std::runtime_error("Invalid BUFFER transfer direct selection.");
    }

    return this->AXIFull_BufferIO(transferConfig, buffer);
}

/* ------------------------------------- AXI-Lite/AXI-Full/XDMA Word IO ------------------------------------------- */

bool vuprs::FPGAController::AXI_XDMA_WordTransfer(const vuprs::DMATransferConfig &transferConfig, uint32_t *r_value, const uint32_t &w_value)
{
    if (!IS_DMA_WORD_TRANSFER_MEMORY_SELECTION(transferConfig.transferMemorySelection))
    {
        throw std::runtime_error("Invalid WORD memory selection.");
    }
    if (!IS_DMA_TRANSFER_DIRECTION(transferConfig.transferDirectionSelection))
    {
        throw std::runtime_error("Invalid BUFFER transfer direct selection.");
    }

    return this->AXI_XDMA_WordIO(transferConfig, w_value, r_value);
}

void vuprs::SetDMATransferConfigToDefault(DMATransferConfig *config)
{
    config->dmaChannel = 0;

    config->base = 0;
    config->offset = 0;
    
    config->transferByteSize = 0;

    config->transferDirectionSelection = DMA_TRANSFER_DIRECTION__FPGA_TO_HOST;
    config->transferMemorySelection = DMA_TRANSFER_MEMORY_SELECTION__DDR;
}