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
#include <filesystem>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "fpga_config.h"
#include "aligned_buffer.h"
#include "log_manager.h"

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

/* AXI-Lite User Access */

#define __AXI_LITE__DMA_USER_ADDRESS              16

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

/* -------------------------------------- AXI-Full DMA Direction ------------------------------------ */

#define DMA_TRANSFER_DIRECTION__FPGA_TO_HOST      0
#define DMA_TRANSFER_DIRECTION__HOST_TO_FPGA      1

#define IS_DMA_TRANSFER_DIRECTION(VAL) (VAL == DMA_TRANSFER_DIRECTION__FPGA_TO_HOST || VAL == DMA_TRANSFER_DIRECTION__HOST_TO_FPGA)

#define DMA_TRANSFER_MEMORY_SELECTION__DDR         0
#define DMA_TRANSFER_MEMORY_SELECTION__BRAM        1
#define DMA_TRANSFER_MEMORY_SELECTION__AXI_LITE_DOMAIN  2
#define DMA_TRANSFER_MEMORY_SELECTION__XDMA_DOMAIN 3

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

#define __LINUX_DMA_MAX_TRANSFER_BYTES__          0x7ffff000  /* Maximum transfer size in Linux-32bit or Linux-64bit */
#define __XDMA_AXI_LITE_MMAP_SIZE__               (2 * 64 * 1024UL)  /* 2 * 64 kB address in VUPRS FPGA AXI-Lite bus address space */
#define __XDMA_CONTROL_MMAP_SIZE__                (32 * 1024UL)  /* 32 kB address mmap */

namespace vuprs
{
    /* -----------------------------------  Aligned Data Structure --------------------------------- */

    struct DMATransferConfig
    {
        uint8_t dmaChannel;
        uint64_t base;  /* Not used in AXI-Full word/buffer transfer */
        uint64_t offset;
        uint64_t transferByteSize;  /* Not used in AXI-Full word transfer */
        int transferMemorySelection;
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
             * @note The JSON file must be the required format.
             * @param configJsonFilename file name of the JSON file. (e.g. ./usr/config.json)
             * @retval true: load data success;
             *         false: load data failed.
             * @throws std::runtime_error
             */
            bool LoadFPGAConfig(const vuprs::FPGAConfigManager &newFPGAConfig);

            void InitLogger(const std::string &loggerName, const std::string &loggerFilename);

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
             * @brief Write/Read data to/from DDR on AXI-Full bus of FPGA (use DMA method).
             * @param transferConfig transfer config parameters.
             * @param buffer send/receive buffer. 
             *               In read mode (DMA_TRANSFER_DIRECTION__FPGA_TO_HOST), the method will
             *               automatically configure the buffer.
             *               In write mode (DMA_TRANSFER_DIRECTION__HOST_TO_FPGA), data must be written
             *               to the buffer in advance.
             * @retval true: write/read success;
             *         false: write/read failed.
             * @throw std::runtime_error, std::bad_malloc
             */
            bool AXIFull_BufferTransfer(const vuprs::DMATransferConfig &transferConfig, vuprs::AlignedBufferDMA *buffer);

            /**
             * @brief Write data to AXI-Full bus.
             * @param dmaChannel DMA channel select.
             * @param offset offset relative to DDR.
             * @param w_value write value
             * @retval true: write success;
             *         false: write failed.
             * @throw std::runtime_error
             */
            bool AXI_XDMA_WordTransfer(const vuprs::DMATransferConfig &transferConfig, uint32_t *r_value, const uint32_t &w_value);
    };

    void SetDMATransferConfigToDefault(DMATransferConfig *config);
}

#endif
