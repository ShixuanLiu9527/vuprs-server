#include "FPGAControl.h"

#define __DIRECTION_IS_READ__(DIR) \
(DIR == "R" || DIR == "READ" || DIR == "RD")

#define __DIRECTION_IS_WRITE__(DIR) \
(DIR == "W" || DIR == "WRITE" || DIR == "WR")

void FreeAll(int fpga_fd, int file_fd, char **allocated);

vuprs::FPGAController::FPGAController()
{
    this->configDown = false;
    this->fpgaConfigJsonFilename = "";
}

vuprs::FPGAController::~FPGAController()
{

}

/* --------------------------------------------------------------------------------------------------------------- */
/* --------------------------------------------- Parse JSON file ------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

bool vuprs::FPGAController::LoadFPGAConfigFromJson(const std::string &configJsonFilename)
{
    if (!std::filesystem::exists(configJsonFilename))
    {
        throw std::runtime_error("No such file: " + configJsonFilename);
    }
    if (fpgaConfig == nullptr)
    {
        throw std::runtime_error("Invalid pointer *fpgaConfig");
    }

    this->fpgaConfigJsonFilename = configJsonFilename;

    std::ifstream configJsonFile;

    /* open config json file */

    configJsonFile.open(configJsonFilename);
    if (!configJsonFile.is_open())
    {
        throw std::runtime_error("Cannot open file: " + configJsonFilename);
    }

    nlohmann::json configJsonData;
    int configSuccessCount = 0;
    configJsonFile >> configJsonData;

    /* security check */

    if (configJsonData.contains("address-map"))
    {
        /* Bus Memory Base Address */
        auto addressMap = configJsonData["address-map"];
        if (this->ParseMemoryBaseAddress(addressMap)) configSuccessCount++;

        if (addressMap.contains("axi-lite"))
        {
            auto addressMapAxiLite = addressMap["axi-lite"];

            if (addressMapAxiLite.contains("adc"))
            {
                /* ADC Register Address */
                if (this->ParseRegisterAddressADC(addressMapAxiLite["adc"])) configSuccessCount++;
            }

            if (addressMapAxiLite.contains("dma"))
            {
                /* DMA Register Address */
                if (this->ParseRegisterAddressDMA(addressMapAxiLite["dma"])) configSuccessCount++;
            }

        }
    }
    if (configJsonData.contains("hardware-features"))
    {
        /* Hardware Features */
        if (this->ParseHardwareFeatures(configJsonData["hardware-features"])) configSuccessCount++;
    }
    if (configJsonData.contains("xdma-driver"))
    {
        /* XDMA Driver */
        if (this->ParseXDMADriverConfig(configJsonData["xdma-driver"])) configSuccessCount++;
    }

    if (configSuccessCount == 5)
    {
        this->configDown = true;
        return true;
    }
    else
    {
        this->configDown = false;
        return false;
    }
}

bool vuprs::FPGAController::ParseMemoryBaseAddress(const nlohmann::json &jsonData)
{

    uint64_t axiLiteBusAddress = 0, adcAddressOffset = 0, dmaAddressOffset = 0;
    uint64_t axiFullBusAddress = 0, ddrAddressOffset = 0;

    uint64_t parseResultValue;
    bool parseHexStatus;

    bool parseSuccess = true;

    if (jsonData.contains("axi-lite") && jsonData.contains("axi-full"))
    {
        auto axiLite = jsonData["axi-lite"];
        auto axiFull = jsonData["axi-full"];

        /* ADC & DMA Bus Address (AXI-Lite) */

        if (axiLite.contains("bus-address-offset") && axiLite.contains("adc") && axiLite.contains("dma"))
        {
            parseResultValue = this->ParseHexFromString(axiLite["bus-address-offset"].get<std::string>(), &parseHexStatus);
            if (parseHexStatus) axiLiteBusAddress = parseResultValue;
            else parseSuccess = false;
            
            auto axiLiteADC = axiLite["adc"];
            auto axiLiteDMA = axiLite["dma"];

            if (axiLiteADC.contains("address-offset")) 
            {
                parseResultValue = this->ParseHexFromString(axiLiteADC["address-offset"].get<std::string>(), &parseHexStatus);
                if (parseHexStatus) adcAddressOffset = parseResultValue;
                else parseSuccess = false;
            }
            else 
            {
                parseSuccess = false;
            }
            
            if (axiLiteDMA.contains("address-offset")) 
            {
                parseResultValue = this->ParseHexFromString(axiLiteDMA["address-offset"].get<std::string>(), &parseHexStatus);
                if (parseHexStatus) dmaAddressOffset = parseResultValue;
                else parseSuccess = false;
            }
            else parseSuccess = false;
        }
        else
        {
            parseSuccess = false;
        }

        /* DDR Bus Address (AXI-Full) */

        if (axiFull.contains("bus-address-offset") && axiFull.contains("ddr"))
        {
            parseResultValue = this->ParseHexFromString(axiFull["bus-address-offset"].get<std::string>(), &parseHexStatus);
            if (parseHexStatus) axiFullBusAddress = parseResultValue;
            else parseSuccess = false;
            
            auto axiFullDDR = axiFull["ddr"];

            if (axiFullDDR.contains("address-offset")) 
            {
                parseResultValue = this->ParseHexFromString(axiFullDDR["address-offset"].get<std::string>(), &parseHexStatus);
                if (parseHexStatus) ddrAddressOffset = parseResultValue;
                else parseSuccess = false;
            }
            else parseSuccess = false;
        }
        else
        {
            parseSuccess = false;
        }
    }
    else
    {
        parseSuccess = false;
    }

    /* DDR Address at AXI-Full */
    this->fpgaConfig->fpgaAddress.busAddress.addrBusBaseAXIFull__DDR = ddrAddressOffset;
    /* ADC Controller Address at AXI-Lite */
    this->fpgaConfig->fpgaAddress.busAddress.addrBusBaseAXILite__ADC = adcAddressOffset;
    /* DMA Controller Address at AXI-Lite */
    this->fpgaConfig->fpgaAddress.busAddress.addrBusBaseAXILite__DMA = dmaAddressOffset;

    this->fpgaConfig->fpgaAddress.busAddress.addrBusBaseAXILite = axiLiteBusAddress;
    this->fpgaConfig->fpgaAddress.busAddress.addrBusBaseAXIFull = axiFullBusAddress;

    return parseSuccess;
}

bool vuprs::FPGAController::ParseRegisterAddressADC(const nlohmann::json &jsonData)
{
    std::vector<std::string> registerAddressList = {
        "SCI-address-offset",
        "SP-address-offset",
        "SF-address-offset",
        "STR-address-offset",
        "NGF-address-offset",
        "ERR-address-offset"
    };

    std::vector<uint64_t*> registerAddressTarget = {
        &this->fpgaConfig->fpgaAddress.registerAddressADC.addrRegisterBaseADC__SCI,
        &this->fpgaConfig->fpgaAddress.registerAddressADC.addrRegisterBaseADC__SP,
        &this->fpgaConfig->fpgaAddress.registerAddressADC.addrRegisterBaseADC__SF,
        &this->fpgaConfig->fpgaAddress.registerAddressADC.addrRegisterBaseADC__STR,
        &this->fpgaConfig->fpgaAddress.registerAddressADC.addrRegisterBaseADC__NGF,
        &this->fpgaConfig->fpgaAddress.registerAddressADC.addrRegisterBaseADC__ERR
    };

    bool parseHexStatus;
    uint64_t parseResultValue;
    int successCount = 0;

    if (jsonData.contains("registers-address-offset"))
    {
        auto adcRegAddr = jsonData["registers-address-offset"];

        for (int i = 0; i < registerAddressList.size(); i++)
        {
            if (adcRegAddr.contains(registerAddressList[i]))
            {
                parseResultValue = this->ParseHexFromString(adcRegAddr[registerAddressList[i]].get<std::string>(), &parseHexStatus);
                if (parseHexStatus) 
                {
                    *(registerAddressTarget[i]) = parseResultValue;
                    successCount++;
                }
            }
        }
    }

    if (successCount == registerAddressList.size())
    {
        return true;
    }

    return false;
}

bool vuprs::FPGAController::ParseRegisterAddressDMA(const nlohmann::json &jsonData)
{
    std::vector<std::string> registerAddressList = {
        "S2MM_DMACR-address-offset",
        "S2MM_DMASR-address-offset",
        "SG_CTL-address-offset",
        "S2MM_CURDESC-address-offset",
        "S2MM_CURDESC_MSB-address-offset",
        "S2MM_TAILDESC-address-offset",
        "S2MM_TAILDESC_MSB-address-offset",
        "S2MM_DA-address-offset",
        "S2MM_DA_MSB-address-offset",
        "S2MM_LENGTH-address-offset"
    };

    std::vector<uint64_t*> registerAddressTarget = {
        &this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_DMACR,
        &this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_DMASR,
        &this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__SG_CTL,
        &this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_CURDESC,
        &this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_CURDESC_MSB,
        &this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_TAILDESC,
        &this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_TAILDESC_MSB,
        &this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_DA,
        &this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_DA_MSB,
        &this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_LENGTH
    };

    bool parseHexStatus;
    uint64_t parseResultValue;
    int successCount = 0;

    if (jsonData.contains("registers-address-offset"))
    {
        auto adcRegAddr = jsonData["registers-address-offset"];

        for (int i = 0; i < registerAddressList.size(); i++)
        {
            if (adcRegAddr.contains(registerAddressList[i]))
            {
                parseResultValue = this->ParseHexFromString(adcRegAddr[registerAddressList[i]].get<std::string>(), &parseHexStatus);
                if (parseHexStatus) 
                {
                    *(registerAddressTarget[i]) = parseResultValue;
                    successCount++;
                }
            }
        }
    }

    if (successCount == registerAddressList.size())
    {
        return true;
    }

    return false;
}

bool vuprs::FPGAController::ParseHardwareFeatures(const nlohmann::json &jsonData)
{
    bool parseIntegerStatus, parseSuccess = true;
    int parseResultValue;

    /* ADC Hardware Features */

    if (jsonData.contains("adc"))
    {
        auto adcHardware = jsonData["adc"];

        /* ADC Data Width */

        if (adcHardware.contains("data-width-bits"))
        {
            parseResultValue = this->ParseIntegerFromString(adcHardware["data-width-bits"].get<std::string>(), &parseIntegerStatus);
            if (parseIntegerStatus) this->fpgaConfig->hardwareConfig.adcDataWidth_bits = parseResultValue;
            else parseSuccess = false;
        }

        /* ADC Channels */

        if (adcHardware.contains("channels"))
        {
            parseResultValue = this->ParseIntegerFromString(adcHardware["channels"].get<std::string>(), &parseIntegerStatus);
            if (parseIntegerStatus) this->fpgaConfig->hardwareConfig.adcChannels = parseResultValue;
            else parseSuccess = false;
        }

        /* ADC Maximum Sampling Frequency */

        if (adcHardware.contains("max-sampling-frequency-hz"))
        {
            parseResultValue = this->ParseIntegerFromString(adcHardware["max-sampling-frequency-hz"].get<std::string>(), &parseIntegerStatus);
            if (parseIntegerStatus) this->fpgaConfig->hardwareConfig.adcMaxSamplingFrequency_Hz = parseResultValue;
            else parseSuccess = false;
        }
    }

    /* DDR Hardware Features */

    if (jsonData.contains("ddr"))
    {
        auto ddrHardware = jsonData["ddr"];

        /* DDR Memory Capacity (MB) */

        if (ddrHardware.contains("memory-capacity-megabytes"))
        {
            parseResultValue = this->ParseIntegerFromString(ddrHardware["memory-capacity-megabytes"].get<std::string>(), &parseIntegerStatus);
            if (parseIntegerStatus) this->fpgaConfig->hardwareConfig.ddrMemoryCapacity_megabytes = parseResultValue;
            else parseSuccess = false;
        }

        /* DDR Data Width */

        if (ddrHardware.contains("data-width-bits"))
        {
            parseResultValue = this->ParseIntegerFromString(ddrHardware["data-width-bits"].get<std::string>(), &parseIntegerStatus);
            if (parseIntegerStatus) this->fpgaConfig->hardwareConfig.ddrDataWidth_bits = parseResultValue;
            else parseSuccess = false;
        }
    }

    return parseSuccess;
}

bool vuprs::FPGAController::ParseXDMADriverConfig(const nlohmann::json &jsonData)
{
    bool parseIntegerStatus, parseSuccess = true;
    int parseResultValue;

    if (jsonData.contains("device-files"))
    {
        auto deviceFiles = jsonData["device-files"];

        if (deviceFiles.contains("xdma-control"))
        {
            this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_control = deviceFiles["xdma-control"].get<std::string>();
        }
        else
        {
            parseSuccess = false;
        }

        if (deviceFiles.contains("xdma-h2c"))
        {
            auto xdmaH2C = deviceFiles["xdma-h2c"];

            if (xdmaH2C.is_array())
            {
                this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_h2c.clear();
                this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_h2c.reserve(xdmaH2C.size() + 1);

                for (int i = 0; i < xdmaH2C.size(); i++)
                {
                    this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_h2c.push_back(xdmaH2C[i].get<std::string>());
                }
            }
            else
            {
                parseSuccess = false;
            }
        }
        else
        {
            parseSuccess = false;
        }

        if (deviceFiles.contains("xdma-c2h"))
        {
            auto xdmaC2H = deviceFiles["xdma-c2h"];

            if (xdmaC2H.is_array())
            {
                this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_c2h.clear();
                this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_c2h.reserve(xdmaC2H.size() + 1);

                for (int i = 0; i < xdmaC2H.size(); i++)
                {
                    this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_c2h.push_back(xdmaC2H[i].get<std::string>());
                }
            }
            {
                parseSuccess = false;
            }
        }
        else
        {
            parseSuccess = false;
        }

        if (deviceFiles.contains("xdma-events"))
        {
            auto xdmaEvents = deviceFiles["xdma-events"];

            if (xdmaEvents.is_array())
            {
                this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_events.clear();
                this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_events.reserve(xdmaEvents.size() + 1);

                for (int i = 0; i < xdmaEvents.size(); i++)
                {
                    this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_events.push_back(xdmaEvents[i].get<std::string>());
                }
            }
            {
                parseSuccess = false;
            }
        }
        {
            parseSuccess = false;
        }
    }
    else
    {
        parseSuccess = false;
    }

    if (jsonData.contains("max-transfer-size-bytes"))
    {
        parseResultValue = this->ParseIntegerFromString(jsonData["max-transfer-size-bytes"].get<std::string>(), &parseIntegerStatus);
        if (parseIntegerStatus) this->fpgaConfig->xdmaDriverConfig.maxTransferSize_bytes = parseResultValue;
        else parseSuccess = false;

        if (this->fpgaConfig->xdmaDriverConfig.maxTransferSize_bytes > __LINUX_DMA_MAX_TRANSFER_BYTES__)
        {
            this->fpgaConfig->xdmaDriverConfig.maxTransferSize_bytes = __LINUX_DMA_MAX_TRANSFER_BYTES__;
        }
    }
    else
    {
        parseSuccess = false;
    }

    return parseSuccess;
}

uint64_t vuprs::FPGAController::ParseHexFromString(const std::string &dataString, bool *status)
{
    std::string hexString = dataString;

    if (status != nullptr)
    {
        *status = false;
    }

    if (dataString.empty() || (dataString.substr(0, 2) != "0X" && dataString.substr(0, 2) != "0x"))
    {
        return 0;
    }
    
    std::transform(hexString.begin(), hexString.end(), hexString.begin(), ::toupper);

    try
    {
        hexString.erase(std::remove(hexString.begin(), hexString.end(), '_'), hexString.end());

        if (hexString.length() >= 3)
        {
            /* Check Digital Value */
            for (size_t i = 2; i < hexString.length(); i++)
            {
                if (!std::isxdigit(hexString[i]))
                {
                    return 0;
                }
            }

            if (status != nullptr)
            {
                *status = true;
            }

            return (uint64_t)(std::stoull(hexString, nullptr, 16));
        }
        else
        {
            return 0;
        }

    } 
    catch (...)
    {
        return 0;
    }
}

int vuprs::FPGAController::ParseIntegerFromString(const std::string &dataString, bool *status)
{
    if (status != nullptr)
    {
        *status = false;
    }
    if (dataString.empty())
    {
        return 0;
    }

    try
    {
        /* Check Digital Value */
        for (size_t i = 0; i < dataString.length(); i++)
        {
            if (!std::isdigit(dataString[i]))
            {
                return 0;
            }
        }
        if (status != nullptr)
        {
            *status = true;
        }
        return (uint64_t)(std::stoull(dataString, nullptr, 10));
    } 
    catch (...)
    {
        return 0;
    }
}

/* --------------------------------------------------------------------------------------------------------------- */
/* ------------------------------------------- FPGA Communication ------------------------------------------------ */
/* --------------------------------------------------------------------------------------------------------------- */

bool vuprs::FPGAController::FPGAConfigDown()
{
    return this->configDown;
}

/* ------------------------------------------- Read/Write to value ----------------------------------------------- */

uint64_t vuprs::FPGAController::AXILite_GetRegisterOffset(const int &registerSelection, bool *status)
{
    if (status != nullptr)
    {
        *status = false;
    }
    if (!IS_AXI_LITE_REGISTER(registerSelection) || !this->configDown)
    {
        return 0;
    }

    uint64_t axiLiteRegisterSpaceBaseAddress = 0, registerOffset = 0;

    /* Calculate base address of the register address space (relative to AXI-Lite base address) */

    if (IS_AXI_LITE_REGISTER__ADC(registerSelection))
    {
        axiLiteRegisterSpaceBaseAddress = this->fpgaConfig->fpgaAddress.busAddress.addrBusBaseAXILite__ADC;
    }
    else if (IS_AXI_LITE_REGISTER__DMA(registerSelection))
    {
        axiLiteRegisterSpaceBaseAddress = this->fpgaConfig->fpgaAddress.busAddress.addrBusBaseAXILite__DMA;
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
            registerOffset = this->fpgaConfig->fpgaAddress.registerAddressADC.addrRegisterBaseADC__SCI;
            break;
        }
        case AXI_LITE_REGISTER__ADC__SP:
        {
            registerOffset = this->fpgaConfig->fpgaAddress.registerAddressADC.addrRegisterBaseADC__SP;
            break;
        }
        case AXI_LITE_REGISTER__ADC__SF:
        {
            registerOffset = this->fpgaConfig->fpgaAddress.registerAddressADC.addrRegisterBaseADC__SF;
            break;
        }
        case AXI_LITE_REGISTER__ADC__STR:
        {
            registerOffset = this->fpgaConfig->fpgaAddress.registerAddressADC.addrRegisterBaseADC__STR;
            break;
        }
        case AXI_LITE_REGISTER__ADC__NGF:
        {
            registerOffset = this->fpgaConfig->fpgaAddress.registerAddressADC.addrRegisterBaseADC__NGF;
            break;
        }
        case AXI_LITE_REGISTER__ADC__ERR:
        {
            registerOffset = this->fpgaConfig->fpgaAddress.registerAddressADC.addrRegisterBaseADC__ERR;
            break;
        }

        /* DMA Controller Registers */

        case AXI_LITE_REGISTER__DMA__S2MM_DMACR:
        {
            registerOffset = this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_DMACR;
            break;
        }
        case AXI_LITE_REGISTER__DMA__S2MM_DMASR:
        {
            registerOffset = this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_DMASR;
            break;
        }
        case AXI_LITE_REGISTER__DMA__SG_CTL:
        {
            registerOffset = this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__SG_CTL;
            break;
        }
        case AXI_LITE_REGISTER__DMA__S2MM_CURDESC:
        {
            registerOffset = this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_CURDESC;
            break;
        }
        case AXI_LITE_REGISTER__DMA__S2MM_CURDESC_MSB:
        {
            registerOffset = this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_CURDESC_MSB;
            break;
        }
        case AXI_LITE_REGISTER__DMA__S2MM_TAILDESC:
        {
            registerOffset = this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_TAILDESC;
            break;
        }
        case AXI_LITE_REGISTER__DMA__S2MM_TAILDESC_MSB:
        {
            registerOffset = this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_TAILDESC_MSB;
            break;
        }
        case AXI_LITE_REGISTER__DMA__S2MM_DA:
        {
            registerOffset = this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_DA;
            break;
        }
        case AXI_LITE_REGISTER__DMA__S2MM_DA_MSB:
        {
            registerOffset = this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_DA_MSB;
            break;
        }
        case AXI_LITE_REGISTER__DMA__S2MM_LENGTH:
        {
            registerOffset = this->fpgaConfig->fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_LENGTH;
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

bool vuprs::FPGAController::AXILite_FPGARegisterIO(const std::string &rd_wr, const int &registerSelection, const uint32_t &w_value, uint32_t *r_value)
{
    /* ------------------------ Security Check Start ------------------------- */

    if (!IS_AXI_LITE_REGISTER(registerSelection))
    {
        throw std::runtime_error("Invalid register selection: " + std::to_string(registerSelection));
    }
    if (!this->configDown)
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
    uint64_t currentOffset = 0, registerOffset = 0;
    
    /* Calculate register address */

    registerOffset = this->AXILite_GetRegisterOffset(registerSelection, &registerCalculateStatus);

    if (!registerCalculateStatus)
    {
        throw std::runtime_error("Invalid register selection.");
    }

    /* Open device file (AXI-Lite) */

    fpga_fd = open((this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_control).c_str(), O_RDWR);

    /* Check if device file is open */

    if (fpga_fd < 0)
    {
        throw std::runtime_error("Cannot open device file: " + this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_control);
    }

    /* Seek to offset relative to AXI-Lite base address in FPGA */

    currentOffset = lseek(fpga_fd, registerOffset, SEEK_SET);

    if (currentOffset != registerOffset)
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
        throw std::runtime_error("Cannot close device file: " + this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_control);
    }

    return writeReadStatus >= 0;
}

bool vuprs::FPGAController::AXIFull_IO(const std::string &rd_wr, const uint64_t &offsetDDR, const std::vector<uint64_t> &writeBuffer, std::vector<uint64_t> *readBuffer, const uint64_t &readBytes, const uint8_t &dmaChannel)
{
    /* ------------------------ Security Check Start ------------------------- */

    if (!this->configDown)  /* detect at first */
    {
        throw std::runtime_error("Config not complete.");
    }

    std::string direction = rd_wr;
    std::transform(direction.begin(), direction.end(), direction.begin(), ::toupper);  /* Upper */

    if (!(__DIRECTION_IS_READ__(direction) || __DIRECTION_IS_WRITE__(direction)))
    {
        throw std::runtime_error("Invalid IO direction.");
    }

    if (__DIRECTION_IS_WRITE__(direction))
    {
        if (writeBuffer.size() <= 0)
        {
            throw std::runtime_error("Write buffer is empty, no data be written.");
        }
        else if (writeBuffer.size() >= this->fpgaConfig->xdmaDriverConfig.maxTransferSize_bytes / sizeof(uint64_t))
        {
            throw std::runtime_error("Write buffer is bigger than maximum transfer size.");
        }
    }

    if (__DIRECTION_IS_READ__(direction))
    {
        if (readBuffer == nullptr)
        {
            throw std::runtime_error("Read buffer is NULL.");
        }
        if (readBytes <= 0)
        {
            throw std::runtime_error("Read bytes is 0.");
        }
    }

    /* ------------------------- Security Check End -------------------------- */

    int fpga_fd = -1, writeReadStatus = -1, writeReadBytes = 0;
    uint64_t currentOffset = 0, componentOffset = 0;
    uint64_t targetWriteReadBytes = 0;

    /* Aligned vector for XDMA */

    std::vector<uint64_t, vuprs::VectorAlignedAllocatorXDMA<uint64_t, __XDMA_DMA_ALIGNMENT_BYTES__>> alignedWriteBuffer, alignedReadBuffer;

    /* Set DDR offset (relative to AXI-Full) */

    componentOffset = this->fpgaConfig->fpgaAddress.busAddress.addrBusBaseAXIFull__DDR + offsetDDR;

    /* Open device file (AXI-Full DMA) */

    if (__DIRECTION_IS_READ__(direction))
    {
        if (dmaChannel >= this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_c2h.size())
        {
            throw std::runtime_error(
                "Invalid DMA channel (required: < " + \
                std::to_string(this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_c2h.size()) + "), current = " + \
                std::to_string(dmaChannel)
            );
        }

        fpga_fd = open((this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_c2h[dmaChannel]).c_str(), O_RDWR);
    }
    else if (__DIRECTION_IS_WRITE__(direction))
    {
        if (dmaChannel >= this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_h2c.size())
        {
            throw std::runtime_error(
                "Invalid DMA channel (required: < " + \
                std::to_string(this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_h2c.size()) + "), current = " + \
                std::to_string(dmaChannel)
            );
        }

        fpga_fd = open((this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_h2c[dmaChannel]).c_str(), O_RDWR);
    }
    else
    {
        throw std::runtime_error("Invalid IO direction.");
    }

    /* Check if device file is open */

    if (fpga_fd < 0)
    {
        throw std::runtime_error("Cannot open device file: " + this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_control);
    }

    /* Seek to offset relative to AXI-Full base address in FPGA */

    currentOffset = lseek(fpga_fd, componentOffset, SEEK_SET);

    if (currentOffset != componentOffset)
    {
        close(fpga_fd);  /* close file */
        throw std::runtime_error("Seek error.");
    }

    /* Write data to DDR */

    if (__DIRECTION_IS_WRITE__(direction))
    {
        uint64_t writeSize = writeBuffer.size();
        alignedWriteBuffer.resize(writeSize);

        /* Copy */

        std::copy(writeBuffer.begin(), writeBuffer.end(), alignedWriteBuffer.begin());

        /* DMA to FPGA */

        targetWriteReadBytes = alignedWriteBuffer.size() * sizeof(uint64_t);
        writeReadBytes = write(fpga_fd, alignedWriteBuffer.data(), targetWriteReadBytes);
    }
    else if (__DIRECTION_IS_READ__(direction))
    {
        uint64_t maxReadBytes = std::min(this->fpgaConfig->xdmaDriverConfig.maxTransferSize_bytes, readBytes);

        if (readBuffer->size() <= 0 || readBuffer->size() >= maxReadBytes / sizeof(uint64_t))
        {
            readBuffer->resize(maxReadBytes / sizeof(uint64_t));
        }

        alignedReadBuffer.resize(readBuffer->size());

        targetWriteReadBytes = alignedReadBuffer.size() * sizeof(uint64_t);
        writeReadBytes = read(fpga_fd, alignedReadBuffer.data(), targetWriteReadBytes);

        /* Copy back */

        std::copy(alignedReadBuffer.begin(), alignedReadBuffer.end(), readBuffer->begin());
    }

    /* Close */

    if (close(fpga_fd) < 0)
    {
        throw std::runtime_error("Cannot close device file: " + this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_control);
    }

    return (static_cast<int>(targetWriteReadBytes) == writeReadBytes);
}

/* ------------------------------------------- Read/Write to file ------------------------------------------------ */

void FreeAll(int fpga_fd, int file_fd, char **allocated)
{
    if (fpga_fd >= 0)
    {
        close(fpga_fd);
    }
    if (file_fd >= 0)
    {
        close(file_fd);
    }
    if (allocated != nullptr)
    {
        if ((*allocated) != nullptr)
        {
#ifdef _WIN32
            _aligned_free(*allocated);
#else
            free(*allocated);
#endif
        }
    }
}

bool vuprs::FPGAController::AXIFull_IO(const std::string &rd_wr, const uint64_t &offsetDDR, const uint64_t &offsetFile, const std::string &inputfileName, const std::string &outputFileName, const uint64_t &transferBytes, const uint8_t &dmaChannel)
{
    /* ------------------------ Security Check Start ------------------------- */

    if (!this->configDown)  /* detect at first */
    {
        throw std::runtime_error("Config not complete.");
    }

    std::string direction = rd_wr;
    std::transform(direction.begin(), direction.end(), direction.begin(), ::toupper);  /* Upper */

    if (!(__DIRECTION_IS_READ__(direction) || __DIRECTION_IS_WRITE__(direction)))
    {
        throw std::runtime_error("Invalid IO direction.");
    }

    if (__DIRECTION_IS_WRITE__(direction))
    {
        if (inputfileName.empty())
        {
            throw std::runtime_error("Invalid input file name.");
        }
    }

    if (__DIRECTION_IS_READ__(direction))
    {
        if (outputFileName.empty())
        {
            throw std::runtime_error("Invalid output file name.");
        }
    }

    if (transferBytes <= 0)
    {
        throw std::runtime_error("Read bytes is 0.");
    }

    if ((offsetDDR + transferBytes) > this->fpgaConfig->hardwareConfig.ddrMemoryCapacity_megabytes * 1024 * 1024)
    {
        throw std::runtime_error("Read Domain of the DDR overflow.");
    }

    /* ------------------------- Security Check End -------------------------- */

    int fpga_fd = -1, file_fd = -1, writeReadStatus = -1, writeReadBytes = 0;
    uint64_t currentOffset = 0, componentOffset = 0, currentFileOffset = 0;
    uint64_t targetWriteReadBytes = 0;
    char* allocated = nullptr;

    /* Create aligned memory domain */

    #ifdef _WIN32
        allocated = static_cast<char*>(_aligned_malloc(transferBytes + __XDMA_DMA_ALIGNMENT_BYTES__, __XDMA_DMA_ALIGNMENT_BYTES__));
    #else
        if(posix_memalign((void **)&allocated, __XDMA_DMA_ALIGNMENT_BYTES__ , transferBytes + __XDMA_DMA_ALIGNMENT_BYTES__) != 0)
        {
            FreeAll(fpga_fd, file_fd, &allocated);
            throw std::runtime_error("Cannot make aligned memory.");
        }
    #endif

    if (allocated == nullptr)
    {
        FreeAll(fpga_fd, file_fd, &allocated);
        throw std::runtime_error("Cannot make aligned memory.");
    }

    /* Open input/output file */

    if (__DIRECTION_IS_READ__(direction))
    {
        file_fd = open(outputFileName.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);

        if (file_fd < 0)
        {
            FreeAll(fpga_fd, file_fd, &allocated);
            throw std::runtime_error("Cannot open file: " + outputFileName);
        }
    }
    else if (__DIRECTION_IS_WRITE__(direction))
    {
        file_fd = open(inputfileName.c_str(), O_RDONLY);

        if (file_fd < 0)
        {
            FreeAll(fpga_fd, file_fd, &allocated);
            throw std::runtime_error("Cannot open file: " + inputfileName);
        }
    }

    /* Open device file (AXI-Full DMA) */

    if (__DIRECTION_IS_READ__(direction))
    {
        if (dmaChannel >= this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_c2h.size())
        {
            FreeAll(fpga_fd, file_fd, &allocated);
            throw std::runtime_error(
                "Invalid DMA channel (required: < " + \
                std::to_string(this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_c2h.size()) + "), current = " + \
                std::to_string(dmaChannel)
            );
        }

        fpga_fd = open((this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_c2h[dmaChannel]).c_str(), O_RDWR);
    }
    else if (__DIRECTION_IS_WRITE__(direction))
    {
        if (dmaChannel >= this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_h2c.size())
        {
            FreeAll(fpga_fd, file_fd, &allocated);
            throw std::runtime_error(
                "Invalid DMA channel (required: < " + \
                std::to_string(this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_h2c.size()) + "), current = " + \
                std::to_string(dmaChannel)
            );
        }

        fpga_fd = open((this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_h2c[dmaChannel]).c_str(), O_RDWR);
    }

    /* Check if device file & input/output file is open */

    if (fpga_fd < 0)
    {
        FreeAll(fpga_fd, file_fd, &allocated);
        throw std::runtime_error("Cannot open device file: " + this->fpgaConfig->xdmaDriverConfig.deviceFilename_xdma_control);
    }

    /* --- Seek --- */

    /* Seek to offset relative to AXI-Full base address in FPGA */

    componentOffset = this->fpgaConfig->fpgaAddress.busAddress.addrBusBaseAXIFull__DDR + offsetDDR;
    currentOffset = lseek(fpga_fd, componentOffset, SEEK_SET);

    if (currentOffset != componentOffset)
    {
        FreeAll(fpga_fd, file_fd, &allocated);
        throw std::runtime_error("Seek error.");
    }

    /* Seek file offset */

    currentFileOffset = lseek(file_fd, offsetFile, SEEK_SET);

    if (currentFileOffset != offsetFile)
    {
        FreeAll(fpga_fd, file_fd, &allocated);
        throw std::runtime_error("Seek error.");
    }

    /* --- Read --- */

    /* Read FPGA data to buffer (READ mode) */

    if (__DIRECTION_IS_READ__(direction))
    {
        writeReadBytes = read(fpga_fd, allocated, transferBytes);

        if (static_cast<int>(transferBytes) != writeReadBytes)
        {
            FreeAll(fpga_fd, file_fd, &allocated);
            throw std::runtime_error("Read from FPGA error.");
        }
    }

    /* Read file data to buffer (WRITE mode) */

    else if (__DIRECTION_IS_WRITE__(direction))
    {
        writeReadBytes = read(file_fd, allocated, transferBytes);

        if (static_cast<int>(transferBytes) != writeReadBytes)
        {
            FreeAll(fpga_fd, file_fd, &allocated);
            throw std::runtime_error("Read from File error.");
        }
    }

    /* --- Write --- */

    /* Write buffer data to file (READ mode) */

    if (__DIRECTION_IS_READ__(direction))
    {
        writeReadBytes = write(file_fd, allocated, transferBytes);

        if (static_cast<int>(transferBytes) != writeReadBytes)
        {
            FreeAll(fpga_fd, file_fd, &allocated);
            throw std::runtime_error("Read from FPGA error.");
        }
    }

    /* Write buffer data to FPGA (WRITE mode) */

    else if (__DIRECTION_IS_WRITE__(direction))
    {
        writeReadBytes = write(fpga_fd, allocated, transferBytes);

        if (static_cast<int>(transferBytes) != writeReadBytes)
        {
            FreeAll(fpga_fd, file_fd, &allocated);
            throw std::runtime_error("Read from File error.");
        }
    }

    /* Free all */

    FreeAll(fpga_fd, file_fd, &allocated);

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
        return this->AXILite_FPGARegisterIO("write", registerSelection, w_value, nullptr);
    }
    else
    {
        throw std::runtime_error("Register is read only: " + std::to_string(registerSelection));
    }
}

bool vuprs::FPGAController::AXILite_ReadFPGARegister(const int &registerSelection, uint32_t *r_value)
{
    return this->AXILite_FPGARegisterIO("read", registerSelection, 0, r_value);
}

/* --------------------------------------------------- AXI-Full -------------------------------------------------- */

/* vector format */

bool vuprs::FPGAController::AXIFull_WriteToFPGA(const std::vector<uint64_t> &writeBuffer, const uint64_t &offsetDDR, const uint8_t &dmaChannel)
{
    return this->AXIFull_IO("write", offsetDDR, writeBuffer, nullptr, 0, dmaChannel);
}

bool vuprs::FPGAController::AXIFull_ReadFromFPGA(std::vector<uint64_t> *readBuffer, const uint64_t &offsetDDR, const uint64_t &readBytes, const uint8_t &dmaChannel)
{
    std::vector<uint64_t> writeBuffer;
    return this->AXIFull_IO("read", offsetDDR, writeBuffer, readBuffer, readBytes, dmaChannel);
}

/* file format */

bool vuprs::FPGAController::AXIFull_WriteToFPGA(const std::string &inputFileName, const uint64_t &offsetDDR, const uint64_t &offsetFile, const uint64_t &writeBytes, const uint8_t &dmaChannel)
{
    return this->AXIFull_IO("write", offsetDDR, offsetFile, inputFileName, "", writeBytes, dmaChannel);
}

bool vuprs::FPGAController::AXIFull_ReadFromFPGA(const std::string &outputFileName, const uint64_t &offsetDDR, const uint64_t &offsetFile, const uint64_t &readBytes, const uint8_t &dmaChannel)
{
    return this->AXIFull_IO("read", offsetDDR, offsetFile, "", outputFileName, readBytes, dmaChannel);
}
