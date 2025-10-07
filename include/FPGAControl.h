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
#include <memory>
#include <cstdlib>
#include <stdexcept>

#ifdef _WIN32
#include <malloc.h>
#else
#include <cstdlib>
#endif

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

#define IS_AXI_LITE_REGISTER__ADC(VAL) \
(VAL == AXI_LITE_REGISTER__ADC__SCI               || \
 VAL == AXI_LITE_REGISTER__ADC__SP                || \
 VAL == AXI_LITE_REGISTER__ADC__SF                || \
 VAL == AXI_LITE_REGISTER__ADC__STR               || \
 VAL == AXI_LITE_REGISTER__ADC__NGF               || \
 VAL == AXI_LITE_REGISTER__ADC__ERR)

#define IS_AXI_LITE_RDONLY_REGISTER(VAL) \
(VAL == AXI_LITE_REGISTER__ADC__NGF               || \
 VAL == AXI_LITE_REGISTER__ADC__ERR)

#define IS_AXI_LITE_REGISTER__DMA(VAL) \
(VAL == AXI_LITE_REGISTER__DMA__S2MM_DMACR        || \
 VAL == AXI_LITE_REGISTER__DMA__S2MM_DMASR        || \
 VAL == AXI_LITE_REGISTER__DMA__SG_CTL            || \
 VAL == AXI_LITE_REGISTER__DMA__S2MM_CURDESC      || \
 VAL == AXI_LITE_REGISTER__DMA__S2MM_CURDESC_MSB  || \
 VAL == AXI_LITE_REGISTER__DMA__S2MM_TAILDESC     || \
 VAL == AXI_LITE_REGISTER__DMA__S2MM_TAILDESC_MSB || \
 VAL == AXI_LITE_REGISTER__DMA__S2MM_DA           || \
 VAL == AXI_LITE_REGISTER__DMA__S2MM_DA_MSB       || \
 VAL == AXI_LITE_REGISTER__DMA__S2MM_LENGTH)

 #define IS_AXI_LITE_REGISTER(VAL) \
 (IS_AXI_LITE_REGISTER__ADC(VAL)                  || \
  IS_AXI_LITE_REGISTER__DMA(VAL))

/* --------------------------------------- Aligned Parameters --------------------------------------- */

#define __XDMA_DMA_ALIGNMENT_BYTES__              4096        /* 4 kB alignment */
#define __LINUX_DMA_MAX_TRANSFER_BYTES__          0x7ffff000  /* Maximum transfer size in Linux-32bit or Linux-64bit */

namespace vuprs
{
    /* -----------------------------------  Aligned Vector ------------------------------------- */

    template<typename T, std::size_t Alignment>
    class VectorAlignedAllocatorXDMA 
    {
        static_assert(Alignment > 0, "Alignment must be positive");
        
        public:
            using value_type = T;
            using pointer = T*;
            using const_pointer = const T*;
            using reference = T&;
            using const_reference = const T&;
            using size_type = std::size_t;
            using difference_type = std::ptrdiff_t;

            static constexpr std::size_t alignment = Alignment;
        
            template<typename U>
            struct rebind
            {
                using other = VectorAlignedAllocatorXDMA<U, Alignment>;
            };
        
            VectorAlignedAllocatorXDMA() noexcept = default;
        
            template<typename U>
            VectorAlignedAllocatorXDMA(const VectorAlignedAllocatorXDMA<U, Alignment>&) noexcept {}
        
            pointer allocate(size_type n) 
            {
                if (n > max_size()) 
                {
                    throw std::bad_alloc();
                }
            
                void* ptr = nullptr;

        #ifdef _WIN32
                ptr = _aligned_malloc(n * sizeof(T), Alignment);
        #else
                if (posix_memalign(&ptr, Alignment, n * sizeof(T)) != 0) 
                {
                    ptr = nullptr;
                }
        #endif

                if (!ptr)
                {
                    throw std::bad_alloc();
                }

                return static_cast<pointer>(ptr);
            }
        
            void deallocate(pointer p, size_type) noexcept 
            {
                if (p) 
                {
        #ifdef _WIN32
                    _aligned_free(p);
        #else
                    std::free(p);
        #endif
                }
            }
        
            size_type max_size() const noexcept 
            {
                return std::size_t(-1) / sizeof(T);
            }
        
            template<typename U, typename... Args>
            void construct(U* p, Args&&... args) 
            {
                ::new(static_cast<void*>(p)) U(std::forward<Args>(args)...);
            }
        
            template<typename U>
            void destroy(U* p) 
            {
                p->~U();
            }
        
            template<typename U>
            bool operator==(const VectorAlignedAllocatorXDMA<U, Alignment>&) const noexcept 
            {
                return true;
            }
        
            template<typename U>
            bool operator!=(const VectorAlignedAllocatorXDMA<U, Alignment>& other) const noexcept 
            {
                return !(*this == other);
            }
    };

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
            
            bool AXILite_FPGARegisterIO(const std::string &rd_wr, const int &registerSelection, const uint32_t &w_value, uint32_t *r_value);

            bool AXIFull_IO(const std::string &rd_wr, const uint64_t &offsetDDR, const std::vector<uint64_t> &writeBuffer, std::vector<uint64_t> *readBuffer, const uint64_t &readBytes, const uint8_t &dmaChannel);
            bool AXIFull_IO(const std::string &rd_wr, const uint64_t &offsetDDR, const uint64_t &offsetFile, const std::string &inputfileName, const std::string &outputFileName, const uint64_t &transferBytes, const uint8_t &dmaChannel);
            
        public:

            FPGAController();

            ~FPGAController();

            /**
             * @brief Load config data from JSON file.
             * @note The JSON file must be the required format.
             * @param configJsonFilename file name of the JSON file. (e.g. ./usr/config.json)
             * @retval true: load data success;
             *         false: load data failed.
             * @throws std::runtime_error
             */
            bool LoadFPGAConfigFromJson(const std::string &configJsonFilename);

            /**
             * @brief Check if the configuration is complete.
             * @note Call FPGAController::LoadFPGAConfigFromJson() before.
             * @retval true: configuration is complete;
             *         false: configuration not complete.
             */
            bool FPGAConfigDown();

            /**
             * @brief Write word (32 bit) to register on AXI-Lite bus of FPGA (use Simple method).
             * @param registerSelection target register.
             * @param w_value value to write.
             * @retval true: write success;
             *         false: write failed.
             * @throw std::runtime_error
             */
            bool AXILite_WriteToFPGARegister(const int &registerSelection, const uint32_t &w_value);

            /**
             * @brief Read word (32 bit) from register on AXI-Lite bus of FPGA (use Simple method).
             * @param registerSelection target register.
             * @param r_value read value.
             * @retval true: read success;
             *         false: read failed.
             * @throw std::runtime_error
             */
            bool AXILite_ReadFPGARegister(const int &registerSelection, uint32_t *r_value);

            /**
             * @brief Write data to DDR on AXI-Full bus of FPGA (use Simple method).
             * @param writeBuffer data to write (cannot be empty).
             * @param offsetDDR offset of DDR (relative to the begining of DDR)
             * @param writeBytes write bytes. (be smaller than XDMA.MAX_TRANSFER_BYTES)
             * @param dmaChannel DMA channel selection, 0: DMA channel, 1: DMA channel 1.
             * @retval true: write success;
             *         false: write failed.
             * @throw std::runtime_error
             */
            bool AXIFull_WriteToFPGA(const std::vector<uint64_t> &writeBuffer, const uint64_t &offsetDDR, const uint8_t &dmaChannel = 0);
            
            /**
             * @brief Write data to DDR on AXI-Full bus of FPGA (use Simple method).
             * @param inputFileName data to write (cannot be empty).
             * @param offsetDDR offset of DDR (relative to the begining of DDR, bytes)
             * @param offsetFile offset of file (relative to the begining of file, bytes)
             * @param writeBytes write bytes. (be smaller than XDMA.MAX_TRANSFER_BYTES)
             * @param dmaChannel DMA channel selection, 0: DMA channel, 1: DMA channel 1.
             * @retval true: write success;
             *         false: write failed.
             * @throw std::runtime_error
             */
            bool AXIFull_WriteToFPGA(const std::string &inputFileName, const uint64_t &offsetDDR, const uint64_t &offsetFile, const uint64_t &writeBytes, const uint8_t &dmaChannel = 0);

            /**
             * @brief Read data from DDR on AXI-Full bus of FPGA (use Simple method).
             * @param readBuffer read buffer to store data.
             * @param offsetDDR offset of DDR (relative to the begining of DDR)
             * @param readBytes read bytes. (be smaller than XDMA.MAX_TRANSFER_BYTES)
             * @param dmaChannel DMA channel selection, 0: DMA channel, 1: DMA channel 1.
             * @retval true: read success;
             *         false: read failed.
             * @throw std::runtime_error
             */
            bool AXIFull_ReadFromFPGA(std::vector<uint64_t> *readBuffer, const uint64_t &offsetDDR, const uint64_t &readBytes, const uint8_t &dmaChannel = 0);

            /**
             * @brief Write data to DDR on AXI-Full bus of FPGA (use Simple method).
             * @param inputFileName data to write (cannot be empty).
             * @param offsetDDR offset of DDR (relative to the begining of DDR, bytes)
             * @param offsetFile offset of file (relative to the begining of file, bytes)
             * @param readBytes write bytes. (be smaller than XDMA.MAX_TRANSFER_BYTES)
             * @param dmaChannel DMA channel selection, 0: DMA channel, 1: DMA channel 1.
             * @retval true: write success;
             *         false: write failed.
             * @throw std::runtime_error
             */
            bool AXIFull_ReadFromFPGA(const std::string &outputFileName, const uint64_t &offsetDDR, const uint64_t &offsetFile, const uint64_t &readBytes, const uint8_t &dmaChannel = 0);
    };
}

#endif
