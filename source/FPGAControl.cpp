#include "FPGAControl.h"

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
    nlohmann::json configJsonData;

    /* open config json file */

    configJsonFile.open(configJsonFilename);
    if (!configJsonFile.is_open())
    {
        throw std::runtime_error("Cannot open file: " + configJsonFilename);
    }
    configJsonFile >> configJsonData;

    /* security check */

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
            parseResultValue = this->ParseHexFromString(axiLite["bus-address-offset"], &parseHexStatus);
            if (parseHexStatus) axiLiteBusAddress = parseResultValue;
            else parseSuccess = false;
            
            auto axiLiteADC = axiLite["adc"];
            auto axiLiteDMA = axiLite["dma"];

            if (axiLiteADC.contains("address-offset")) 
            {
                parseResultValue = this->ParseHexFromString(axiLiteADC["address-offset"], &parseHexStatus);
                if (parseHexStatus) adcAddressOffset = parseResultValue;
                else parseSuccess = false;
            }
            else 
            {
                parseSuccess = false;
            }
            
            if (axiLiteDMA.contains("address-offset")) 
            {
                parseResultValue = this->ParseHexFromString(axiLiteDMA["address-offset"], &parseHexStatus);
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
            parseResultValue = this->ParseHexFromString(axiFull["bus-address-offset"], &parseHexStatus);
            if (parseHexStatus) axiFullBusAddress = parseResultValue;
            else parseSuccess = false;
            
            auto axiFullDDR = axiFull["ddr"];

            if (axiFullDDR.contains("address-offset")) 
            {
                parseResultValue = this->ParseHexFromString(axiFullDDR["address-offset"], &parseHexStatus);
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
    this->fpgaConfig->fpgaAddress.busAddress.addrBusBaseAXIFull__DDR = axiFullBusAddress + ddrAddressOffset;
    /* ADC Controller Address at AXI-Lite */
    this->fpgaConfig->fpgaAddress.busAddress.addrBusBaseAXILite__ADC = axiLiteBusAddress + adcAddressOffset;
    /* DMA Controller Address at AXI-Lite */
    this->fpgaConfig->fpgaAddress.busAddress.addrBusBaseAXILite__DMA = axiLiteBusAddress + dmaAddressOffset;

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
                parseResultValue = this->ParseHexFromString(adcRegAddr[registerAddressList[i]], &parseHexStatus);
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
                parseResultValue = this->ParseHexFromString(adcRegAddr[registerAddressList[i]], &parseHexStatus);
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
            parseResultValue = this->ParseIntegerFromString(adcHardware["data-width-bits"], &parseIntegerStatus);
            if (parseIntegerStatus) this->fpgaConfig->adcDataWidth_bits = parseResultValue;
            else parseSuccess = false;
        }

        /* ADC Channels */

        if (adcHardware.contains("channels"))
        {
            parseResultValue = this->ParseIntegerFromString(adcHardware["channels"], &parseIntegerStatus);
            if (parseIntegerStatus) this->fpgaConfig->adcChannels = parseResultValue;
            else parseSuccess = false;
        }

        /* ADC Maximum Sampling Frequency */

        if (adcHardware.contains("max-sampling-frequency-hz"))
        {
            parseResultValue = this->ParseIntegerFromString(adcHardware["max-sampling-frequency-hz"], &parseIntegerStatus);
            if (parseIntegerStatus) this->fpgaConfig->adcMaxSamplingFrequency_Hz = parseResultValue;
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
            parseResultValue = this->ParseIntegerFromString(ddrHardware["memory-capacity-megabytes"], &parseIntegerStatus);
            if (parseIntegerStatus) this->fpgaConfig->ddrMemoryCapacity_megabytes = parseResultValue;
            else parseSuccess = false;
        }

        /* DDR Data Width */

        if (ddrHardware.contains("data-width-bits"))
        {
            parseResultValue = this->ParseIntegerFromString(ddrHardware["data-width-bits"], &parseIntegerStatus);
            if (parseIntegerStatus) this->fpgaConfig->ddrDataWidth_bits = parseResultValue;
            else parseSuccess = false;
        }
    }

    return parseSuccess;
}
