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
#include <fcntl.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>

#include "fpga_config.h"
#include "aligned_data_structure.h"

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

#define IS_DMA_TRANSFER_DIRECTION(VAL) \
(VAL == DMA_TRANSFER_DIRECTION__FPGA_TO_HOST      || \
 VAL == DMA_TRANSFER_DIRECTION__HOST_TO_FPGA)

/* ----------------------------------- Fixed Transfer Parameters ------------------------------------ */

#define __LINUX_DMA_MAX_TRANSFER_BYTES__          0x7ffff000  /* Maximum transfer size in Linux-32bit or Linux-64bit */

namespace vuprs
{
    /* -----------------------------------  Aligned Data Structure --------------------------------- */

    typedef struct DMATransferConfig
    {
        uint8_t transferDmaChannel;
        uint64_t ddrOffset;
        uint64_t transferByteSize;
        int transferDirectionSelection;
    };
    
    /* ----------------------------------  FPGA Controller ------------------------------------ */

    class FPGAController
    {
        private:
            vuprs::FPGAConfigManager fpgaConfigManager;

            uint64_t AXILite_GetRegisterOffset(const int &registerSelection, bool *status = nullptr);
            bool AXILite_FPGARegisterIO(const std::string &rd_wr, const int &registerSelection, const uint32_t &w_value, uint32_t *r_value, const uint64_t &base, const uint64_t &offset);

            bool AXIFull_BufferIO(const vuprs::DMATransferConfig &transferConfig, vuprs::AlignedBufferDMA *buffer);

        public:

            FPGAController();

            FPGAController(const std::string &configJsonFilename);

            ~FPGAController();

            /**
             * @brief Load config data from JSON file.
             * @note The JSON file must be the required format.
             * @param configJsonFilename file name of the JSON file. (e.g. ./usr/config.json)
             * @retval true: load data success;
             *         false: load data failed.
             * @throws std::runtime_error
             */
            bool LoadFPGAConfig(const vuprs::FPGAConfigManager &newFPGAConfig);

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
            bool AXIFull_IO(const vuprs::DMATransferConfig &transferConfig, vuprs::AlignedBufferDMA *buffer);

            /**
             * @brief Read data on AXI-Lite bus.
             * @param base base address of the memory space (relative to AXI-Lite).
             * @param offset offset relative to <base>.
             * @retval true: write success;
             *         false: write failed.
             * @throw std::runtime_error
             */
            bool AXILite_Read(const uint64_t &base, const uint64_t &offset, uint32_t *r_value);

            /**
             * @brief Write data on AXI-Lite bus.
             * @param base base address of the memory space (relative to AXI-Lite).
             * @param offset offset relative to <base>.
             * @retval true: write success;
             *         false: write failed.
             * @throw std::runtime_error
             */
            bool AXILite_Write(const uint64_t &base, const uint64_t &offset, const uint32_t &w_value);
    };
}

#endif
