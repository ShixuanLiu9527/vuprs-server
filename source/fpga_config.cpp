#include "fpga_config.h"

vuprs::FPGAConfigManager::FPGAConfigManager()
{
    this->configDown = false;
}

vuprs::FPGAConfigManager::~FPGAConfigManager()
{

}

bool vuprs::FPGAConfigManager::ConfigDown() const
{
    return this->configDown;
}

/* --------------------------------------------------------------------------------------------------------------- */
/* --------------------------------------------- Parse JSON file ------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------------------- */

bool vuprs::FPGAConfigManager::LoadFPGAConfigFromJson(const std::string &configJsonFilename)
{
    if (!std::filesystem::exists(configJsonFilename))
    {
        throw std::runtime_error("No such file: " + configJsonFilename);
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
        this->fpgaConfig.configdown = true;
        return true;
    }
    else
    {
        this->configDown = false;
        this->fpgaConfig.configdown = false;
        return false;
    }
}

bool vuprs::FPGAConfigManager::ParseMemoryBaseAddress(const nlohmann::json &jsonData)
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
    this->fpgaConfig.fpgaAddress.busAddress.addrBusBaseAXIFull__DDR = ddrAddressOffset;
    /* ADC Controller Address at AXI-Lite */
    this->fpgaConfig.fpgaAddress.busAddress.addrBusBaseAXILite__ADC = adcAddressOffset;
    /* DMA Controller Address at AXI-Lite */
    this->fpgaConfig.fpgaAddress.busAddress.addrBusBaseAXILite__DMA = dmaAddressOffset;

    this->fpgaConfig.fpgaAddress.busAddress.addrBusBaseAXILite = axiLiteBusAddress;
    this->fpgaConfig.fpgaAddress.busAddress.addrBusBaseAXIFull = axiFullBusAddress;

    this->fpgaConfig.fpgaAddress.busAddress.configdown = true;

    return parseSuccess;
}

bool vuprs::FPGAConfigManager::ParseRegisterAddressADC(const nlohmann::json &jsonData)
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
        &this->fpgaConfig.fpgaAddress.registerAddressADC.addrRegisterBaseADC__SCI,
        &this->fpgaConfig.fpgaAddress.registerAddressADC.addrRegisterBaseADC__SP,
        &this->fpgaConfig.fpgaAddress.registerAddressADC.addrRegisterBaseADC__SF,
        &this->fpgaConfig.fpgaAddress.registerAddressADC.addrRegisterBaseADC__STR,
        &this->fpgaConfig.fpgaAddress.registerAddressADC.addrRegisterBaseADC__NGF,
        &this->fpgaConfig.fpgaAddress.registerAddressADC.addrRegisterBaseADC__ERR
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
        this->fpgaConfig.fpgaAddress.registerAddressADC.configdown = true;
        return true;
    }

    this->fpgaConfig.fpgaAddress.registerAddressADC.configdown = false;
    return false;
}

bool vuprs::FPGAConfigManager::ParseRegisterAddressDMA(const nlohmann::json &jsonData)
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
        &this->fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_DMACR,
        &this->fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_DMASR,
        &this->fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__SG_CTL,
        &this->fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_CURDESC,
        &this->fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_CURDESC_MSB,
        &this->fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_TAILDESC,
        &this->fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_TAILDESC_MSB,
        &this->fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_DA,
        &this->fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_DA_MSB,
        &this->fpgaConfig.fpgaAddress.registerAddressDMA.addrRegisterBaseDMA__S2MM_LENGTH
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
        this->fpgaConfig.fpgaAddress.registerAddressDMA.configdown = true;
        return true;
    }

    this->fpgaConfig.fpgaAddress.registerAddressDMA.configdown = false;
    return false;
}

bool vuprs::FPGAConfigManager::ParseHardwareFeatures(const nlohmann::json &jsonData)
{
    bool parseIntegerStatus, parseSuccessADC = true, parseSuccessDDR = true;
    int parseResultValue;

    /* ADC Hardware Features */

    if (jsonData.contains("adc"))
    {
        auto adcHardware = jsonData["adc"];

        /* ADC Data Width */

        if (adcHardware.contains("data-width-bits"))
        {
            parseResultValue = this->ParseIntegerFromString(adcHardware["data-width-bits"].get<std::string>(), &parseIntegerStatus);
            if (parseIntegerStatus) this->fpgaConfig.hardwareConfig.hardwareConfigADC.adcDataWidth_bits = parseResultValue;
            else parseSuccessADC = false;
        }

        /* ADC Channels */

        if (adcHardware.contains("channels"))
        {
            parseResultValue = this->ParseIntegerFromString(adcHardware["channels"].get<std::string>(), &parseIntegerStatus);
            if (parseIntegerStatus) this->fpgaConfig.hardwareConfig.hardwareConfigADC.adcChannels = parseResultValue;
            else parseSuccessADC = false;
        }

        /* ADC Maximum Sampling Frequency */

        if (adcHardware.contains("max-sampling-frequency-hz"))
        {
            parseResultValue = this->ParseIntegerFromString(adcHardware["max-sampling-frequency-hz"].get<std::string>(), &parseIntegerStatus);
            if (parseIntegerStatus) this->fpgaConfig.hardwareConfig.hardwareConfigADC.adcMaxSamplingFrequency_Hz = parseResultValue;
            else parseSuccessADC = false;
        }

        /* ADC Voltage Range Radius */

        if (adcHardware.contains("voltage-range-radius-v"))
        {
            parseResultValue = this->ParseIntegerFromString(adcHardware["voltage-range-radius-v"].get<std::string>(), &parseIntegerStatus);
            if (parseIntegerStatus) this->fpgaConfig.hardwareConfig.hardwareConfigADC.adcVoltageRangeRadius = static_cast<double>(parseResultValue);
            else parseSuccessADC = false;
        }

        /* ADC Frame Size */

        if (adcHardware.contains("frame-length-bytes"))
        {
            parseResultValue = this->ParseIntegerFromString(adcHardware["frame-length-bytes"].get<std::string>(), &parseIntegerStatus);
            if (parseIntegerStatus) this->fpgaConfig.hardwareConfig.hardwareConfigADC.adcFrameSizeBytes = parseResultValue;
            else parseSuccessADC = false;
        }

        if (parseSuccessADC)
        {
            this->fpgaConfig.hardwareConfig.hardwareConfigADC.configdown = true;
        }
        else
        {
            this->fpgaConfig.hardwareConfig.hardwareConfigADC.configdown = false;
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
            if (parseIntegerStatus) this->fpgaConfig.hardwareConfig.hardwareConfigDDR.ddrMemoryCapacity_megabytes = parseResultValue;
            else parseSuccessDDR = false;
        }

        /* DDR Data Width */

        if (ddrHardware.contains("data-width-bits"))
        {
            parseResultValue = this->ParseIntegerFromString(ddrHardware["data-width-bits"].get<std::string>(), &parseIntegerStatus);
            if (parseIntegerStatus) this->fpgaConfig.hardwareConfig.hardwareConfigDDR.ddrDataWidth_bits = parseResultValue;
            else parseSuccessDDR = false;
        }

        if (parseSuccessDDR)
        {
            this->fpgaConfig.hardwareConfig.hardwareConfigDDR.configdown = true;
        }
        else
        {
            this->fpgaConfig.hardwareConfig.hardwareConfigDDR.configdown = false;
        }
    }

    return parseSuccessADC && parseSuccessDDR;
}

bool vuprs::FPGAConfigManager::ParseXDMADriverConfig(const nlohmann::json &jsonData)
{
    bool parseIntegerStatus, parseSuccess = true;
    int parseResultValue;

    if (jsonData.contains("device-files"))
    {
        auto deviceFiles = jsonData["device-files"];

        if (deviceFiles.contains("xdma-control"))
        {
            this->fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_control = deviceFiles["xdma-control"].get<std::string>();
        }
        else
        {
            parseSuccess = false;
        }

        if (deviceFiles.contains("xdma-user"))
        {
            this->fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_user = deviceFiles["xdma-user"].get<std::string>();
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
                this->fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_h2c.clear();
                this->fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_h2c.reserve(xdmaH2C.size() + 1);

                for (int i = 0; i < xdmaH2C.size(); i++)
                {
                    this->fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_h2c.push_back(xdmaH2C[i].get<std::string>());
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
                this->fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_c2h.clear();
                this->fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_c2h.reserve(xdmaC2H.size() + 1);

                for (int i = 0; i < xdmaC2H.size(); i++)
                {
                    this->fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_c2h.push_back(xdmaC2H[i].get<std::string>());
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
                this->fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_events.clear();
                this->fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_events.reserve(xdmaEvents.size() + 1);

                for (int i = 0; i < xdmaEvents.size(); i++)
                {
                    this->fpgaConfig.xdmaDriverConfig.deviceFilename_xdma_events.push_back(xdmaEvents[i].get<std::string>());
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
        if (parseIntegerStatus) this->fpgaConfig.xdmaDriverConfig.maxTransferSize_bytes = parseResultValue;
        else parseSuccess = false;
    }
    else
    {
        parseSuccess = false;
    }

    if (parseSuccess)
    {
        this->fpgaConfig.xdmaDriverConfig.configdown = true;
    }
    else
    {
        this->fpgaConfig.xdmaDriverConfig.configdown = false;
    }

    return parseSuccess;
}

uint64_t vuprs::FPGAConfigManager::ParseHexFromString(const std::string &dataString, bool *status)
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

int vuprs::FPGAConfigManager::ParseIntegerFromString(const std::string &dataString, bool *status)
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
