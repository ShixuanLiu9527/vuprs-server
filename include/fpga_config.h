/**
 * @brief   This document is the configuration interface for FPGA.
 * @version 1.0
 * @author  Shixuan Liu, Tongji University
 * @date    2025-9
 */

#ifndef FPGA_CONFIG_H
#define FPGA_CONFIG_H

#include <stdint.h>
#include <string>
#include <fstream>
#include <iostream>

#include "nlohmann/json.hpp"

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

        bool configdown;
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

        bool configdown;
    };

    typedef struct FPGAregisterAddressADC
    {
        uint64_t addrRegisterBaseADC__SCI;  /* Sampling Clock Increment */
        uint64_t addrRegisterBaseADC__SP;  /* Sampling Points */
        uint64_t addrRegisterBaseADC__SF;  /* Sampling Frames */
        uint64_t addrRegisterBaseADC__STR;  /* Sampling Trigger & Ready */
        uint64_t addrRegisterBaseADC__NGF;  /* Number of Generated Frames */
        uint64_t addrRegisterBaseADC__ERR;  /* Error Flags of ADC */

        bool configdown;
    };

    typedef struct FPGAaddressConfig
    {
        vuprs::FPGAbusAddress busAddress;
        vuprs::FPGAregisterAddressDMA registerAddressDMA;
        vuprs::FPGAregisterAddressADC registerAddressADC;

        bool configdown;
    };

    typedef struct FPGAhardwareConfigDDR
    {
        uint64_t ddrMemoryCapacity_megabytes;
        uint64_t ddrDataWidth_bits;

        bool configdown;
    };

    typedef struct FPGAhardwareConfigADC
    {

        uint64_t adcMaxSamplingFrequency_Hz;
        double adcVoltageRangeRadius;

        bool configdown;
    };

    typedef struct FPGAhardwareConfig
    {
        /* DDR Parameters */

        FPGAhardwareConfigDDR hardwareConfigDDR;

        /* ADC Parameters */

        FPGAhardwareConfigADC hardwareConfigADC;

        bool configdown;
    };
    
    typedef struct XDMADriverConfig
    {
        std::string deviceFilename_xdma_control;
        std::string deviceFilename_xdma_user;
        std::vector<std::string> deviceFilename_xdma_h2c;
        std::vector<std::string> deviceFilename_xdma_c2h;
        std::vector<std::string> deviceFilename_xdma_events;

        uint64_t maxTransferSize_bytes;

        bool configdown;
    };
    

    typedef struct FPGAConfig
    {
        vuprs::FPGAaddressConfig fpgaAddress;  /* Address */
        vuprs::FPGAhardwareConfig hardwareConfig;  /* Hardware */
        vuprs::XDMADriverConfig xdmaDriverConfig;  /* XDMA */

        bool configdown;
    };

    class FPGAConfigManager
    {
        private:

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

        public:

            vuprs::FPGAConfig fpgaConfig;
            std::string fpgaConfigJsonFilename;
            
            FPGAConfigManager();

            ~FPGAConfigManager();

            /**
             * @brief Load config data from JSON file.
             * @note The JSON file must be the required format.
             * @param configJsonFilename file name of the JSON file. (e.g. ./usr/config.json)
             * @retval true: load data success;
             *         false: load data failed.
             * @throws std::runtime_error
             */
            bool LoadFPGAConfigFromJson(const std::string &configJsonFilename);

            bool ConfigDown() const;
    };

    uint64_t ParseHexFromString(const std::string &dataString, bool *status);
    int ParseIntegerFromString(const std::string &dataString, bool *status);

    uint64_t ParseNumberFromString(const std::string &dataString, bool *status);
}

#endif
