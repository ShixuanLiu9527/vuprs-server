#ifndef FPGA_CONTROLLER_H
#define FPGA_CONTROLLER_H

#include <unordered_map>
#include "fpga_device.h"
#include "fpga_memory.h"

#define FPGA_MODULE_COUNT 9U  /* = 5 Devices + 4 Memories */

#define FPGA_REG_BIT(REG, BIT) ((REG) & (uint32_t)((uint32_t)0x00000001 << (BIT)))

namespace vuprs
{
    class FPGAController
    {
        private:

            std::vector<std::shared_ptr<vuprs::FPGA_IOManager>> ioManagerList;
            bool configdone;

            /**
             * @brief Bind IO manager to devices & memories.
             */
            bool BindIOManager();

            /**
             * @brief Get FPGA_IOManager obj index in this->ioManagerList of the certain device filename.
             * 
             * @param deviceFile device filename.
             * @param index index of corrsponding io manager in this->ioManagerList.
             * 
             * @throw std::runtime_error
             */
            void GetOrCreateIOManagerIndex(const std::string &deviceFile, int *index);

        public:

            vuprs::AlignedBufferDMA buffer;  /* aligned buffer for user */

            /* FPGA Devices */

            vuprs::FPGA_Device__AXIDirectMemoryAccess dev__AXI_DMA;  /* AXI Direct Memory Access */
            vuprs::FPGA_Device__ADCController dev__ADC_Controller;  /* ADC Controller */
            vuprs::FPGA_Device__CircularBuffer dev__Circular_Buffer;  /* Circular Buffer */
            vuprs::FPGA_Device__FIRFilterBank dev__FIR_Filter_Bank;  /* FIR Filer Bank */
            vuprs::FPGA_Device__PreDelayUnit dev__PreDelay_Unit;  /* Pre-delay Unit */

            /* FPGA Memories */

            vuprs::FPGA_Memory__DDR mem__DDR;  /* System DDR in FPGA */
            vuprs::FPGA_Memory__FIRBram mem__FIR_BRAM;  /* FIR Coefficient BRAM */
            vuprs::FPGA_Memory__SGBram mem__SG_BRAM;  /* AXI DMA SG BRAM */
            vuprs::FPGA_Memory__CircularBufferBram mem__Circular_Buffer_BRAM;  /* Circular Buffer BRAM */

            /* Interfaces */

            FPGAController(const FPGAController&) = delete;
            FPGAController(FPGAController&&) = delete;

            FPGAController& operator=(const FPGAController&) = delete;
            FPGAController& operator=(FPGAController&&) = delete;

            FPGAController();
            ~FPGAController();

            /**
             * @brief Configure the FPGA using JSON file in constructor.
             * 
             * @note 1st: Load JSON info;
             * @note 2nd: Open device files in FPGA IO Manager;
             * @note 3rd: Bind IO Manager to certain device.
             * @note No need to call method: ConfigFPGAFromJson().
             * 
             * @param configJsonFilename the JSON file name.
             * 
             * @retval true: success.
             * @retval false: failed.
             * 
             * @throw std::runtime_error
             */
            FPGAController(const std::string &configJsonFilename);

            /**
             * @brief Configure the FPGA using JSON file.
             * 
             * @note 1st: Load JSON info;
             * @note 2nd: Open device files in FPGA IO Manager;
             * @note 3rd: Bind IO Manager to certain device.
             * 
             * @param configJsonFilename the JSON file name.
             * 
             * @retval true: success.
             * @retval false: failed.
             * 
             * @throw std::runtime_error
             */
            bool ConfigFPGAFromJson(const std::string &configJsonFilename);

            /**
             * @brief Indicate config is down.
             */
            bool ConfigDown() const;
    };
}

#endif
