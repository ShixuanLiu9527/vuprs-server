#ifndef FPGA_CONTROL_H
#define FPGA_CONTROL_H

#include <stdint.h>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>  /* Standard: C++ 17 */

#include "nlohmann/json.hpp"

namespace vuprs
{

    /* -------------------------------- Address of Registers ------------------------------------ */

    typedef struct FPGAbusAddress
    {
        /* Base Address */

        uint64_t addrBusBaseAXILite__DMA;  /* Offset of AXI-Lite interface for DMA controller configuration */
        uint64_t addrBusBaseAXILite__ADC;  /* Offset of AXI-Lite interface for ADC peripheral control */
        uint64_t addrBusBaseAXIFull__DDR;  /* Offset of AXI-Full interface for DDR3 memory access */
    };

    typedef struct FPGAregisterAddressDMA
    {
        /* Registers Address of DMA SG & Simple Mode (refer to Xilinx PG021 AXI DMA) */

        uint64_t addrRegisterBaseDMA__S2MM_DMACR;  /* S2MM DMA Control register */
        uint64_t addrRegisterBaseDMA__S2MM_DMASR;  /* S2MM DMA Status register */

        /* Registers Address of DMA SG Mode (refer to Xilinx PG021 AXI DMA) */

        /* MM2S channel is excluded */

        uint64_t addrRegisterBaseDMA__SG_CTL;  /* Scatter/Gather User and Cache */
        uint64_t addrRegisterBaseDMA__S2MM_CURDESC;  /* S2MM Current Descriptor Pointer. Lower 32 address bits */
        uint64_t addrRegisterBaseDMA__S2MM_CURDESC_MSB;  /* S2MM Current Descriptor Pointer. Upper 32 address bits */
        uint64_t addrRegisterBaseDMA__S2MM_TAILDESC;  /* S2MM Tail Descriptor Pointer. Lower 32 address bits */
        uint64_t addrRegisterBaseDMA__S2MM_TAILDESC_MSB;  /* S2MM Tail Descriptor Pointer. Upper 32 address bits */

        /* Registers Address of DMA Simple Mode (refer to Xilinx PG021 AXI DMA) */

        uint64_t addrRegisterBaseDMA__S2MM_DA;  /* S2MM Destination Address. Lower 32 bit address */
        uint64_t addrRegisterBaseDMA__S2MM_DA_MSB;  /* S2MM Destination Address. Upper 32 bit address */
        uint64_t addrRegisterBaseDMA__S2MM_LENGTH;  /* S2MM Buffer Length (Bytes) */
    };

    typedef struct FPGAregisterAddressADC
    {
        uint64_t addrRegisterBaseADC__SCI;  /* Sampling Clock Increment */
        uint64_t addrRegisterBaseADC__SP;  /* Sampling Points */
        uint64_t addrRegisterBaseADC__SF;  /* Sampling Frames */
        uint64_t addrRegisterBaseADC__STR;  /* Sampling Trigger & Ready */
        uint64_t addrRegisterBaseADC__NGF;  /* Number of Generated Frames */
        uint64_t addrRegisterBaseADC__ERR;  /* Error Flags of ADC */
    };

    typedef struct FPGAaddressConfig
    {
        vuprs::FPGAbusAddress busAddress;
        vuprs::FPGAregisterAddressDMA registerAddressDMA;
        vuprs::FPGAregisterAddressADC registerAddressADC;
    };

    typedef struct FPGAConfig
    {

        /* Address */
        
        vuprs::FPGAaddressConfig fpgaAddress;

        /* DDR Parameters */

        uint64_t ddrMemoryCapacity_megabytes;
        uint64_t ddrDataWidth_bits;

        /* ADC Parameters */

        uint64_t adcChannels;
        uint64_t adcDataWidth_bits;
        uint64_t adcMaxSamplingFrequency_Hz;
    };

    class FPGAController
    {

    private:

        std::string fpgaConfigJsonFilename;

        /* input json["address-map"] */
        bool ParseMemoryBaseAddress(const nlohmann::json &jsonData);

        /* input json["address-map"]["axi-lite"]["adc"] */
        bool ParseRegisterAddressADC(const nlohmann::json &jsonData);

        /* input json["address-map"]["axi-lite"]["dma"] */
        bool ParseRegisterAddressDMA(const nlohmann::json &jsonData);

        /* input json["hardware-features"] */
        bool ParseHardwareFeatures(const nlohmann::json &jsonData);

        uint64_t ParseHexFromString(const std::string &dataString, bool *status = nullptr);
        int ParseIntegerFromString(const std::string &dataString, bool *status = nullptr);

    public:

        vuprs::FPGAConfig *fpgaConfig;

        FPGAController();

        ~FPGAController();

        bool LoadFPGAConfigFromJson(const std::string &configJsonFilename);

    };

    /* -------------------------------- Functions for Loading ------------------------------------ */

}

#endif
