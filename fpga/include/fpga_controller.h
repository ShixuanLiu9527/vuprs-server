#ifndef FPGA_CONTROLLER_H
#define FPGA_CONTROLLER_H

#include <unordered_map>
#include "fpga_device.h"
#include "fpga_memory.h"

#define FPGA_MODULE_COUNT 9U  /* = 5 Devices + 4 Memories */

namespace vuprs
{
    class FPGAController
    {
        private:

            std::vector<vuprs::FPGA_IOManager> ioManagerList;
            bool configdown;

            bool BindIOManager();

            bool GenerateNewIOManager(const std::string &deviceFile, int *index);

        public:

            /* FPGA Devices */

            vuprs::FPGA_Device__AXIDirectMemoryAccess dev__AXI_DMA;
            vuprs::FPGA_Device__ADCController dev__ADC_Controller;
            vuprs::FPGA_Device__CircularBuffer dev__Circular_Buffer;
            vuprs::FPGA_Device__FIRFilterBank dev__FIR_Filter_Bank;
            vuprs::FPGA_Device__PreDelayUnit dev__PreDelay_Unit;

            /* FPGA Memories */

            vuprs::FPGA_Memory__DDR mem__DDR;
            vuprs::FPGA_Memory__FIRBram mem__FIR_BRAM;
            vuprs::FPGA_Memory__SGBram mem__SG_BRAM;
            vuprs::FPGA_Memory__CircularBufferBram mem__Circular_Buffer_BRAM;

            /* Interfaces */

            FPGAController(const FPGAController&) = delete;
            FPGAController(FPGAController&&) = delete;

            FPGAController& operator=(const FPGAController&) = delete;
            FPGAController& operator=(FPGAController&&) = delete;

            FPGAController();
            FPGAController(const std::string &configJsonFilename);
            ~FPGAController();

            bool ConfigFPGAFromJson(const std::string &configJsonFilename);

            bool ConfigDown() const;
    };
}

#endif
