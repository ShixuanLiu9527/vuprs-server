#ifndef FPGA_CONTROL_H
#define FPGA_CONTROL_H

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>  /* Standard: C++ 17 */

#include "nlohmann/json.hpp"

/* --------------------------------------- AXI-Lite Registers --------------------------------------- */

/* AXI-Lite ADC Registers */

#define AXI_LITE_REGISTER__ADC__SCI               0
#define AXI_LITE_REGISTER__ADC__SP                1
#define AXI_LITE_REGISTER__ADC__SF                2
#define AXI_LITE_REGISTER__ADC__STR               3
#define AXI_LITE_REGISTER__ADC__NGF               4
#define AXI_LITE_REGISTER__ADC__ERR               5

/* AXI-Lite DMA Registers */

#define AXI_LITE_REGISTER__DMA__S2MM_DMACR        6
#define AXI_LITE_REGISTER__DMA__S2MM_DMASR        7
#define AXI_LITE_REGISTER__DMA__SG_CTL            8
#define AXI_LITE_REGISTER__DMA__S2MM_CURDESC      9
#define AXI_LITE_REGISTER__DMA__S2MM_CURDESC_MSB  10
#define AXI_LITE_REGISTER__DMA__S2MM_TAILDESC     11
#define AXI_LITE_REGISTER__DMA__S2MM_TAILDESC_MSB 12
#define AXI_LITE_REGISTER__DMA__S2MM_DA           13
#define AXI_LITE_REGISTER__DMA__S2MM_DA_MSB       14
#define AXI_LITE_REGISTER__DMA__S2MM_LENGTH       15

#define IS_AXI_LITE_REGISTER__ADC(val) \
(val == AXI_LITE_REGISTER__ADC__SCI               || \
 val == AXI_LITE_REGISTER__ADC__SP                || \
 val == AXI_LITE_REGISTER__ADC__SF                || \
 val == AXI_LITE_REGISTER__ADC__STR               || \
 val == AXI_LITE_REGISTER__ADC__NGF               || \
 val == AXI_LITE_REGISTER__ADC__ERR)

#define IS_AXI_LITE_RDONLY_REGISTER(val) \
(val == AXI_LITE_REGISTER__ADC__NGF               || \
 val == AXI_LITE_REGISTER__ADC__ERR)

#define IS_AXI_LITE_REGISTER__DMA(val) \
(val == AXI_LITE_REGISTER__DMA__S2MM_DMACR        || \
 val == AXI_LITE_REGISTER__DMA__S2MM_DMASR        || \
 val == AXI_LITE_REGISTER__DMA__SG_CTL            || \
 val == AXI_LITE_REGISTER__DMA__S2MM_CURDESC      || \
 val == AXI_LITE_REGISTER__DMA__S2MM_CURDESC_MSB  || \
 val == AXI_LITE_REGISTER__DMA__S2MM_TAILDESC     || \
 val == AXI_LITE_REGISTER__DMA__S2MM_TAILDESC_MSB || \
 val == AXI_LITE_REGISTER__DMA__S2MM_DA           || \
 val == AXI_LITE_REGISTER__DMA__S2MM_DA_MSB       || \
 val == AXI_LITE_REGISTER__DMA__S2MM_LENGTH)

 #define IS_AXI_LITE_REGISTER(val) \
 (IS_AXI_LITE_REGISTER__ADC(val)                  || \
  IS_AXI_LITE_REGISTER__DMA(val))

namespace vuprs
{
    /* --------------------------------  FPGA Configuration ------------------------------------ */

    typedef struct FPGAbusAddress
    {
        /* Base Address */

        uint64_t addrBusBaseAXILite;
        uint64_t addrBusBaseAXIFull;

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

    typedef struct FPGAhardwareConfig
    {
        /* DDR Parameters */

        uint64_t ddrMemoryCapacity_megabytes;
        uint64_t ddrDataWidth_bits;

        /* ADC Parameters */

        uint64_t adcChannels;
        uint64_t adcDataWidth_bits;
        uint64_t adcMaxSamplingFrequency_Hz;
    };
    
    typedef struct XDMADriverConfig
    {
        std::string deviceFilename_xdma_control;
        std::vector<std::string> deviceFilename_xdma_h2c;
        std::vector<std::string> deviceFilename_xdma_c2h;
        std::vector<std::string> deviceFilename_xdma_events;

        uint64_t maxTransferSize_bytes;
    };
    

    typedef struct FPGAConfig
    {
        vuprs::FPGAaddressConfig fpgaAddress;  /* Address */
        vuprs::FPGAhardwareConfig hardwareConfig;  /* Hardware */
        vuprs::XDMADriverConfig xdmaDriverConfig;  /* XDMA */
    };

    /* ----------------------------------  FPGA Controller ------------------------------------ */

    class FPGAController
    {
        private:

            vuprs::FPGAConfig *fpgaConfig;

            std::string fpgaConfigJsonFilename;
            bool configDown;

            /* --------------------------- Json read -------------------------------- */

            /* input json["address-map"] */
            bool ParseMemoryBaseAddress(const nlohmann::json &jsonData);

            /* input json["address-map"]["axi-lite"]["adc"] */
            bool ParseRegisterAddressADC(const nlohmann::json &jsonData);

            /* input json["address-map"]["axi-lite"]["dma"] */
            bool ParseRegisterAddressDMA(const nlohmann::json &jsonData);

            /* input json["hardware-features"] */
            bool ParseHardwareFeatures(const nlohmann::json &jsonData);

            /* input json["xdma-driver"] */
            bool ParseXDMADriverConfig(const nlohmann::json &jsonData);

            /* -------------------------- Parse Digital --------------------------- */

            uint64_t ParseHexFromString(const std::string &dataString, bool *status = nullptr);
            int ParseIntegerFromString(const std::string &dataString, bool *status = nullptr);

            uint64_t AXILite_GetRegisterOffset(const int &registerSelection, bool *status = nullptr);
            
            bool DMA_AXILite_FPGARegisterIO(const std::string &rd_wr, const int &registerSelection, const uint32_t &w_value, uint32_t *r_value);

            bool DMA_AXIFull_IO(const std::string &rd_wr, const std::vector<uint64_t> &writeBuffer, std::vector<uint64_t> *readBuffer, const uint64_t &readBytes, const uint8_t &dmaChannel);
            
        public:

            FPGAController();

            ~FPGAController();

            bool LoadFPGAConfigFromJson(const std::string &configJsonFilename);
            bool FPGAConfigDown();

            bool DMA_AXILite_WriteToFPGARegister(const int &registerSelection, const uint32_t &w_value);
            bool DMA_AXILite_ReadFPGARegister(const int &registerSelection, uint32_t *r_value);

            bool DMA_AXIFull_WriteToFPGA(const std::vector<uint64_t> &writeBuffer, const uint8_t &dmaChannel = 0);
            bool DMA_AXIFull_ReadFromFPGA(std::vector<uint64_t> *readBuffer, const uint64_t &readBytes, const uint8_t &dmaChannel = 0);
    };
}

#endif
