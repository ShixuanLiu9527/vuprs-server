/**
 * @brief   This document is the interface for host (RK3568) and card (FPGA) communication.
 * @version 1.0
 * @author  Shixuan Liu, Tongji University
 * @date    2025-9
 */

#ifndef FPGA_CONTROL_H
#define FPGA_CONTROL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>

#ifndef _WIN32
#include <sys/mman.h>
#endif

#include <fcntl.h>

#include "fpga_config.h"
#include "aligned_buffer.h"
#include "log_manager.h"

/* --------------------------------------- AXI-Lite Registers --------------------------------------- */

/**
 * @defgroup AXI_LITE_REGISTERS__ADC
 * @brief AXI-Lite ADC Registers selection
 * @{
 */

#define AXI_LITE_REGISTER__ADC__SCI               0x0000
#define AXI_LITE_REGISTER__ADC__SP                0x0001
#define AXI_LITE_REGISTER__ADC__SF                0x0002
#define AXI_LITE_REGISTER__ADC__STR               0x0003
#define AXI_LITE_REGISTER__ADC__NGF               0x0004
#define AXI_LITE_REGISTER__ADC__ERR               0x0005
#define AXI_LITE_REGISTER__ADC__RST               0x0006

/**
 * @}
 */

/**
 * @defgroup AXI_LITE_REGISTER__DMA
 * @brief AXI-Lite DMA Registers selection
 * @{
 */

#define AXI_LITE_REGISTER__DMA__S2MM_DMACR        0x0007
#define AXI_LITE_REGISTER__DMA__S2MM_DMASR        0x0008
#define AXI_LITE_REGISTER__DMA__SG_CTL            0x0009
#define AXI_LITE_REGISTER__DMA__S2MM_CURDESC      0x000A
#define AXI_LITE_REGISTER__DMA__S2MM_CURDESC_MSB  0x000B
#define AXI_LITE_REGISTER__DMA__S2MM_TAILDESC     0x000C
#define AXI_LITE_REGISTER__DMA__S2MM_TAILDESC_MSB 0x000D
#define AXI_LITE_REGISTER__DMA__S2MM_DA           0x000E
#define AXI_LITE_REGISTER__DMA__S2MM_DA_MSB       0x000F
#define AXI_LITE_REGISTER__DMA__S2MM_LENGTH       0x0010

/**
 * @}
 */

/* AXI_LITE_REGISTER__DMA Parameter check */

#define IS_AXI_LITE_REGISTER__ADC(VAL) \
(VAL == AXI_LITE_REGISTER__ADC__SCI               || \
 VAL == AXI_LITE_REGISTER__ADC__SP                || \
 VAL == AXI_LITE_REGISTER__ADC__SF                || \
 VAL == AXI_LITE_REGISTER__ADC__STR               || \
 VAL == AXI_LITE_REGISTER__ADC__NGF               || \
 VAL == AXI_LITE_REGISTER__ADC__ERR               || \
 VAL == AXI_LITE_REGISTER__ADC__RST)

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

/* -------------------------------------- AXI-Full DMA Direction ------------------------------------ */

/**
 * @defgroup DMA_TRANSFER_DIRECTION
 * @brief DMA transfer direction
 * @{
 */

#define DMA_TRANSFER_DIRECTION__FPGA_TO_HOST                   0x0000
#define DMA_TRANSFER_DIRECTION__HOST_TO_FPGA                   0x0001

/**
 * @}
 */

/* DMA_TRANSFER_DIRECTION Parameter check */

#define IS_DMA_TRANSFER_DIRECTION(VAL) \
(VAL == DMA_TRANSFER_DIRECTION__FPGA_TO_HOST || \
 VAL == DMA_TRANSFER_DIRECTION__HOST_TO_FPGA)

/**
 * @defgroup DMA_TRANSFER_MEMORY_SELECTION
 * @brief DMA transfer memory selection
 * @{
 */

#define DMA_TRANSFER_MEMORY_SELECTION__DDR                     0x0000
#define DMA_TRANSFER_MEMORY_SELECTION__BRAM                    0x0001
#define DMA_TRANSFER_MEMORY_SELECTION__AXI_LITE_DOMAIN         0x0002
#define DMA_TRANSFER_MEMORY_SELECTION__XDMA_DOMAIN             0x0003

/**
 * @}
 */

/* DMA_TRANSFER_MEMORY_SELECTION Parameter check */

#define IS_DMA_WORD_TRANSFER_MEMORY_SELECTION(VAL) \
(VAL == DMA_TRANSFER_MEMORY_SELECTION__DDR        || \
 VAL == DMA_TRANSFER_MEMORY_SELECTION__BRAM       || \
 VAL == DMA_TRANSFER_MEMORY_SELECTION__AXI_LITE_DOMAIN || \
 VAL == DMA_TRANSFER_MEMORY_SELECTION__XDMA_DOMAIN)

#define IS_DMA_BUFFER_TRANSFER_MEMORY_SELECTION(VAL) \
(VAL == DMA_TRANSFER_MEMORY_SELECTION__DDR        || \
 VAL == DMA_TRANSFER_MEMORY_SELECTION__BRAM)

#define IS_DMA_TRANSFER_DIRECTION(VAL) \
(VAL == DMA_TRANSFER_DIRECTION__FPGA_TO_HOST      || \
 VAL == DMA_TRANSFER_DIRECTION__HOST_TO_FPGA)

/* ----------------------------------- Fixed Transfer Parameters ------------------------------------ */

#define __LINUX_DMA_MAX_TRANSFER_BYTES__          0x7ffff000         /* Maximum transfer size in Linux-32bit or Linux-64bit */
#define __XDMA_AXI_LITE_MMAP_SIZE__               (2 * 64 * 1024UL)  /* 2 * 64 kB address in VUPRS FPGA AXI-Lite bus address space */
#define __XDMA_CONTROL_MMAP_SIZE__                (32 * 1024UL)      /* 32 kB address mmap */

namespace vuprs
{
    /* -----------------------------------  Aligned Data Structure --------------------------------- */

    struct DMATransferConfig
    {
        /**
         * @brief DMA Channel.
         * 
         * @note Not used in AXI-Lite word/buffer transfer (AXI-Lite use MMAP instead of DMA)
         * @note For AXI-Full word/buffer transfer, the device file name in fpga_config.json must be used.
         *       (xdma0_c2h_{dmaChannel} & xdma0_h2c_{dmaChannel})
         */
        uint8_t dmaChannel;

        /**
         * @brief Transfer base address.
         * 
         * @note 1. Not used in AXI-Full word/buffer transfer.
         *          For DDR access, equal to 0x00000000. 
         *          For BRAM access, equal to 0x60000000.
         * @note 2. Full address = base + offset.
         */
        uint64_t base;

        /**
         * @brief Transfer offset in FPGA.
         * 
         * @note For AXI-Full word/buffer DDR transfer, valid offset = 0x0000_0000 - 0x1FFF_FFFF.
         * @note For AXI-Full word/buffer BRAM transfer, valid offset = 0x600_00000 - 0x6000_1FFF.
         * @note Full address = base + offset.
         */
        uint64_t offset;

        /**
         * @brief Transfer length (unit is bytes)
         * 
         * @note Cannot exceed __LINUX_DMA_MAX_TRANSFER_BYTES__.
         * @note Not used in AXI-Full/Lite word transfer (equal to 4 bytes).
         */
        uint64_t transferByteSize;

        /**
         * @brief Transfer memory selection
         * 
         * @note valid value are defined in DMA_TRANSFER_MEMORY_SELECTION.
         */
        int transferMemorySelection;

        /**
         * @brief Transfer direction FPGA to Host (reading) or Host to FPGA (writing).
         * 
         * @note Valid values are defined in DMA_TRANSFER_DIRECTION.
         */
        int transferDirectionSelection;
    };
    
    /* ----------------------------------  FPGA Controller ------------------------------------ */

    class FPGAController
    {
        private:
        
            vuprs::FPGAConfigManager fpgaConfigManager;
            std::shared_ptr<spdlog::logger> fpgaControllerLogger;

            uint64_t AXILite_GetRegisterOffset(const int &registerSelection, bool *status);

            bool AXI_XDMA_WordIO(const vuprs::DMATransferConfig &transferConfig, const uint32_t &w_value, uint32_t *r_value);

            bool AXIFull_BufferIO(const vuprs::DMATransferConfig &transferConfig, vuprs::AlignedBufferDMA *buffer);

            /* log */

            void Info(const std::string &info);
            void Warn(const std::string &warn);
            void Error(const std::string &err);
            void Critical(const std::string &critical);

        public:

            FPGAController();

            FPGAController(const std::string &configJsonFilename);

            ~FPGAController();

            FPGAController(const FPGAController&) = delete;
            FPGAController& operator=(const FPGAController&) = delete;

            /**
             * @brief Load config data from JSON file.
             * 
             * @note The JSON file must be the required format.
             * 
             * @param configJsonFilename file name of the JSON file. (e.g. ./usr/config.json)
             * 
             * @retval true: load data success;
             * @retval false: load data failed.
             * 
             * @throws std::runtime_error
             */
            bool LoadFPGAConfig(const vuprs::FPGAConfigManager &newFPGAConfig);

            void InitLogger(const std::string &loggerName, const std::string &loggerFilename);

            /* ---------------------------- Register IO ----------------------------------- */

            /**
             * @brief Write word (32 bit) to register on AXI-Lite bus of FPGA (use Simple method).
             * 
             * @param registerSelection target register, see AXI_LITE_REGISTER__ADC and AXI_LITE_REGISTER__DMA.
             * @param w_value value to write.
             * 
             * @retval true: write success;
             * @retval false: write failed.
             * 
             * @throw std::runtime_error
             */
            bool AXILite_WriteRegister(const int &registerSelection, const uint32_t &w_value);
            
            /**
             * @brief Write 0 or 1 to a certain bit of the register.
             * 
             * @param registerSelection target register, see AXI_LITE_REGISTER__ADC and AXI_LITE_REGISTER__DMA.
             * @param whichBits which bit to write. 0, 1, 2, ...
             * @param value write value, 0 or 1.
             * 
             * @throw std::runtime_error
             */
            bool AXILite_WriteRegister(const int &registerSelection, const uint32_t &whichBits, const bool &value);

            /**
             * @brief Write value to a certain interval of the register.
             * 
             * @note The function will write the value into the interval [lowerBits, upperBits],
             *       and containing both lowerBits and upperBits.
             * 
             * @param registerSelection target register, see AXI_LITE_REGISTER__ADC and AXI_LITE_REGISTER__DMA.
             * @param lowerBits lower bits of the interval.
             * @param upperBits upper bits of the interval.
             * @param value write value.
             * 
             * @throw std::runtime_error
             */
            bool AXILite_WriteRegister(const int &registerSelection, const uint32_t &lowerBits, const uint32_t &upperBits, const uint32_t &value);

            /**
             * @brief Read word (32 bit) from register on AXI-Lite bus of FPGA (use Simple method).
             * 
             * @param registerSelection target register. see AXI_LITE_REGISTER__ADC and AXI_LITE_REGISTER__DMA.
             * @param r_value read value.
             * 
             * @retval true: read success;
             * @retval false: read failed.
             * 
             * @throw std::runtime_error
             */
            bool AXILite_ReadRegister(const int &registerSelection, uint32_t *r_value);

            /**
             * @brief Read certain bit of the register on AXI-Lite bus of FPGA (use Simple method).
             * 
             * @param registerSelection target register. see AXI_LITE_REGISTER__ADC and AXI_LITE_REGISTER__DMA.
             * @param whichBit which bit to read. 0, 1, 2, ...
             * @param r_value read value of the bit.
             * 
             * @retval true: read success;
             * @retval false: read failed.
             * 
             * @throw std::runtime_error
             */
            bool AXILite_ReadRegister(const int &registerSelection, const uint32_t &whichBit, uint32_t *r_value);

            /**
             * @brief Read certain bits interval of the register on AXI-Lite bus of FPGA (use Simple method).
             * @note The function will read the value into the interval [lowerBits, upperBits],
             *       and containing both lowerBits and upperBits.
             * 
             * @param registerSelection target register. see AXI_LITE_REGISTER__ADC and AXI_LITE_REGISTER__DMA.
             * @param lowerBits lower bits of the interval.
             * @param upperBits upper bits of the interval.
             * @param r_value read value of the bits interval.
             * 
             * @retval true: read success;
             * @retval false: read failed.
             * 
             * @throw std::runtime_error
             */
            bool AXILite_ReadRegister(const int &registerSelection, const uint32_t &lowerBits, const uint32_t &upperBits, uint32_t *r_value);

            /* ---------------------------- AXI-Full Transfer -------------------------------- */

            /**
             * @brief Write/Read data to/from DDR/BRAM on AXI-Full bus of FPGA (use DMA method).
             * 
             * @param transferConfig transfer config.
             * @param buffer send/receive buffer. 
             *               In reading mode (DMA_TRANSFER_DIRECTION__FPGA_TO_HOST), the method will
             *               automatically configure the buffer.
             *               In writing mode (DMA_TRANSFER_DIRECTION__HOST_TO_FPGA), data must be written
             *               to the buffer in advance.
             * @retval true: write/read success;
             * @retval false: write/read failed.
             * 
             * @throw std::runtime_error
             * @throw std::bad_malloc
             */
            bool AXIFull_BufferTransfer(const vuprs::DMATransferConfig &transferConfig, vuprs::AlignedBufferDMA *buffer);

            /* --------------------- AXI Word Transfer (for any address) -------------------- */

            /**
             * @brief Read/Write word (32 bit) from/to AXI-Full/Lite bus of FPGA.
             * 
             * @param transferConfig transfer config.
             * @param r_value read value (if not used, pass in nullptr).
             * @param w_value write value (if not used, pass in 0).
             * 
             * @retval true: write success;
             * @retval false: write failed.
             * 
             * @throw std::runtime_error
             */
            bool AXI_XDMA_WordTransfer(const vuprs::DMATransferConfig &transferConfig, uint32_t *r_value, const uint32_t &w_value);
    };

    /**
     * @brief Set struct DMATransferConfig to default value.
     */
    void SetDMATransferConfigToDefault(DMATransferConfig *config);

    /**
     * @brief Calculate optimal value of register SCI for the given target frequency.
     * 
     * @note This function will calculate an optimal value for SCI register.
     *       If target frequency == 0, the output will be 0xffffffff.
     * 
     * @param targetSamplingFreq target sampling frequency.
     * 
     * @retval optimal value of SCI.
     */
    uint32_t GetOptimalValueSCI(const double &targetSamplingFreq);
}

#endif
