#include "fpga_control.h"

#define __DIRECTION_IS_READ__(DIR) \
(DIR == "R" || DIR == "READ" || DIR == "RD")

#define __DIRECTION_IS_WRITE__(DIR) \
(DIR == "W" || DIR == "WRITE" || DIR == "WR")

void FreeAll(int fpga_fd, int file_fd, char **allocated);

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

bool vuprs::FPGAController::AXILite_FPGARegisterIO(
    const std::string &rd_wr, const int &registerSelection, 
    const uint32_t &w_value, uint32_t *r_value, 
    const uint64_t &base, const uint64_t &offset)
{
    /* ------------------------ Security Check Start ------------------------- */

    if (!IS_AXI_LITE_REGISTER(registerSelection) && registerSelection != __AXI_LITE__DMA_USER_ADDRESS)
    {
        throw std::runtime_error("Invalid register selection: " + std::to_string(registerSelection));
    }
    if (!this->fpgaConfigManager.ConfigDown())
    {
        throw std::runtime_error("Config not complete.");
    }

    std::string direction = rd_wr;

    std::transform(direction.begin(), direction.end(), direction.begin(), ::toupper);  /* Upper */

    if (!(__DIRECTION_IS_READ__(direction) || __DIRECTION_IS_WRITE__(direction)))
    {
        throw std::runtime_error("Invalid IO direction.");
    }
    else if (__DIRECTION_IS_WRITE__(direction))
    {
        if (IS_AXI_LITE_RDONLY_REGISTER(registerSelection))
        {
            throw std::runtime_error("This register is read only: " + std::to_string(registerSelection));
        }
    }

    /* ------------------------- Security Check End -------------------------- */

    int fpga_fd = -1, writeReadStatus = -1;
    bool registerCalculateStatus = false;
    uint64_t registerTargetOffset = 0;
    ssize_t currentOffset = -1;
    
    /* Calculate register address */

    if (registerSelection != __AXI_LITE__DMA_USER_ADDRESS)
    {
        registerTargetOffset = this->AXILite_GetRegisterOffset(registerSelection, &registerCalculateStatus);
    }
    else
    {
        registerTargetOffset = base + offset;
    }

    if (!registerCalculateStatus)
    {
        throw std::runtime_error("Invalid register selection.");
    }

    /* Open device file (AXI-Lite) */

#ifdef _WIN32

    fpga_fd = open((this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_user).c_str(), O_RDWR | O_BINARY);

#else

    fpga_fd = open((this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_user).c_str(), O_RDWR);

#endif

    /* Check if device file is open */

    if (fpga_fd < 0)
    {
        throw std::runtime_error("Cannot open device file: " + this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_control);
    }

    /* Seek to offset relative to AXI-Lite base address in FPGA */

    currentOffset = lseek(fpga_fd, registerTargetOffset, SEEK_SET);

    if (static_cast<uint64_t>(currentOffset) != registerTargetOffset || currentOffset < 0 || currentOffset == (off_t) - 1)
    {
        close(fpga_fd);  /* close file */
        throw std::runtime_error("Seek error.");
    }

    /* Write data to register */

    if (__DIRECTION_IS_WRITE__(direction))
    {
        writeReadStatus = write(fpga_fd, &w_value, sizeof(uint32_t));  /* All registers are 32 bit */
    }
    else if (__DIRECTION_IS_READ__(direction))
    {
        if (r_value != nullptr)
        {
            writeReadStatus = read(fpga_fd, r_value, sizeof(uint32_t));  /* All registers are 32 bit */
        }
    }

    /* Close */

    if (close(fpga_fd) < 0)
    {
        throw std::runtime_error("Cannot close device file: " + this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_control);
    }

    return writeReadStatus >= 0;
}

bool vuprs::FPGAController::AXIFull_BufferIO(const vuprs::DMATransferConfig &transferConfig, vuprs::AlignedBufferDMA *buffer)
{
    /* ------------------------ Security Check Start ------------------------- */

    if (!this->fpgaConfigManager.ConfigDown())  /* detect at first */
    {
        throw std::runtime_error("Config not complete.");
    }

    if (!IS_DMA_TRANSFER_DIRECTION(transferConfig.transferDirectionSelection))
    {
        throw std::runtime_error("Invalid direction.");
    }

    if (transferConfig.transferByteSize == 0)
    {
        throw std::runtime_error("Read bytes is 0.");
    }

    if ((transferConfig.ddrOffset + transferConfig.transferByteSize) > this->fpgaConfigManager.fpgaConfig.hardwareConfig.hardwareConfigDDR.ddrMemoryCapacity_megabytes * 1024 * 1024)
    {
        throw std::runtime_error("Read Domain of the DDR overflow.");
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
        if (transferConfig.transferDmaChannel >= this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_c2h.size())
        {
            throw std::runtime_error(
                "Invalid DMA channel (required: < " + \
                std::to_string(this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_c2h.size()) + "), current = " + \
                std::to_string(transferConfig.transferDmaChannel)
            );
        }

        /* Open */

#ifdef _WIN32

        fpga_fd = open((this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_c2h[transferConfig.transferDmaChannel]).c_str(), O_RDWR | O_BINARY);

#else

        fpga_fd = open((this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_c2h[transferConfig.transferDmaChannel]).c_str(), O_RDWR);

#endif

        /* Check if device file & input/output file is open */

        if (fpga_fd < 0)
        {
            throw std::runtime_error("Cannot open device file: " + this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_c2h[transferConfig.transferDmaChannel]);
        }
    }

    else if (transferConfig.transferDirectionSelection == DMA_TRANSFER_DIRECTION__HOST_TO_FPGA)
    {
        if (transferConfig.transferDmaChannel >= this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_h2c.size())
        {
            throw std::runtime_error(
                "Invalid DMA channel (required: < " + \
                std::to_string(this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_h2c.size()) + "), current = " + \
                std::to_string(transferConfig.transferDmaChannel)
            );
        }

#ifdef _WIN32

        fpga_fd = open((this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_h2c[transferConfig.transferDmaChannel]).c_str(), O_RDWR | O_BINARY);
    
#else

        fpga_fd = open((this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_h2c[transferConfig.transferDmaChannel]).c_str(), O_RDWR);

#endif

        /* Check if device file & input/output file is open */

        if (fpga_fd < 0)
        {
            throw std::runtime_error("Cannot open device file: " + this->fpgaConfigManager.fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_h2c[transferConfig.transferDmaChannel]);
        }
    }

    /* --- Seek --- */

    /* Seek to offset relative to AXI-Full base address in FPGA */

    componentOffset = this->fpgaConfigManager.fpgaConfig.fpgaAddress.busAddress.addrBusBaseAXIFull__DDR + transferConfig.ddrOffset;
    currentOffset = lseek(fpga_fd, componentOffset, SEEK_SET);

    if (static_cast<uint64_t>(currentOffset) != componentOffset || 
        currentOffset < 0 || 
        currentOffset == (off_t) - 1)
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
    if (!IS_AXI_LITE_RDONLY_REGISTER(registerSelection))
    {
        return this->AXILite_FPGARegisterIO("write", registerSelection, w_value, nullptr, 0, 0);
    }
    else
    {
        throw std::runtime_error("Register is read only: " + std::to_string(registerSelection));
    }
}

bool vuprs::FPGAController::AXILite_ReadFPGARegister(const int &registerSelection, uint32_t *r_value)
{
    return this->AXILite_FPGARegisterIO("read", registerSelection, 0, r_value, 0, 0);
}

bool vuprs::FPGAController::AXILite_Read(const uint64_t &base, const uint64_t &offset, uint32_t *r_value)
{
    return this->AXILite_FPGARegisterIO("read", __AXI_LITE__DMA_USER_ADDRESS, 0, r_value, base, offset);
}

bool vuprs::FPGAController::AXILite_Write(const uint64_t &base, const uint64_t &offset, const uint32_t &w_value)
{
    return this->AXILite_FPGARegisterIO("write", __AXI_LITE__DMA_USER_ADDRESS, w_value, nullptr, base, offset);
}

/* --------------------------------------------------- AXI-Full -------------------------------------------------- */

bool vuprs::FPGAController::AXIFull_IO(const vuprs::DMATransferConfig &transferConfig, vuprs::AlignedBufferDMA *buffer)
{
    return this->AXIFull_BufferIO(transferConfig, buffer);
}
