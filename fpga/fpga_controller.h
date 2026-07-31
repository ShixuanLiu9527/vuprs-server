#ifndef FPGA_CONTROLLER_H
#define FPGA_CONTROLLER_H

#include <unordered_map>
#include "fpga/fpga_device.h"
#include "fpga/fpga_memory.h"

#define FPGA_MODULE_COUNT 9U /* = 5 Devices + 4 Memories */

namespace vuprs
{
    /**
     * @brief FPGA controller.
     *
     * @note Thread safety.
     */
    class FPGAController
    {
    private:
        std::vector<std::shared_ptr<vuprs::FPGA_IOManagerForInterrput>> io_manager_list_irq;
        std::vector<std::shared_ptr<vuprs::FPGA_IOManagerForDevice>> io_manager_list_dev;
        std::vector<std::shared_ptr<vuprs::FPGA_IOManagerForMemory>> io_manager_list_mem;
        std::atomic<bool> config_done;

        std::mutex mut;

        /**
         * @brief Bind IO manager to devices & memories.
         */
        bool BindIOManager();

        /**
         * @brief Get FPGA_IOManager obj index in this->io_manager_list_dev or this->io_manager_list_mem of the certain device filename.
         *
         * @param device_file device filename.
         * @param index index of corrsponding io manager in this->ioManagerList.
         * @param is_device true: for device, false: for memory.
         *
         * @throw std::runtime_error
         */
        void GetOrCreateNormalIOManagerIndex(const std::string &device_file, int *index, bool is_device);

        /**
         * @brief (For interrupt) Get FPGA_IOManager obj index in this->io_manager_list_irq of the certain device filename.
         *
         * @param device_file device filename.
         * @param index index of corrsponding io manager in this->ioManagerList.
         *
         * @throw std::runtime_error
         */
        void GetOrCreateInterruptIOManagerIndex(const std::string &device_file, int *index);

        void ResetController();

    public:
        /* FPGA Devices */

        vuprs::FPGA_Device__AXIDirectMemoryAccess dev__axi_dma;  /* AXI Direct Memory Access */
        vuprs::FPGA_Device__ADCController dev__adc_controller;   /* ADC Controller */
        vuprs::FPGA_Device__CircularBuffer dev__circular_buffer; /* Circular Buffer */
        vuprs::FPGA_Device__FIRFilterBank dev__fir_filter_bank;  /* FIR Filer Bank */
        vuprs::FPGA_Device__PreDelayUnit dev__predelay_unit;     /* Pre-delay Unit */

        /* FPGA Memories */

        vuprs::FPGA_Memory__DDR mem__ddr;                                  /* System DDR in FPGA */
        vuprs::FPGA_Memory__FIRBram mem__fir_bram;                         /* FIR Coefficient BRAM */
        vuprs::FPGA_Memory__SGBram mem__sg_bram;                           /* AXI DMA SG BRAM */
        vuprs::FPGA_Memory__CircularBufferBram mem___circular_buffer_bram; /* Circular Buffer BRAM */

        /* Interfaces */

        FPGAController(const FPGAController &) = delete;
        FPGAController(FPGAController &&) = delete;

        FPGAController &operator=(const FPGAController &) = delete;
        FPGAController &operator=(FPGAController &&) = delete;

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
         * @param json_filename the JSON file name.
         *
         * @retval true: success.
         * @retval false: failed.
         *
         * @throw std::runtime_error
         */
        FPGAController(const std::string &json_filename);

        /**
         * @brief Configure the FPGA using JSON file.
         *
         * @note 1st: Load JSON info;
         * @note 2nd: Open device files in FPGA IO Manager;
         * @note 3rd: Bind IO Manager to certain device.
         *
         * @param json_filename the JSON file name.
         *
         * @retval true: success.
         * @retval false: failed.
         *
         * @throw std::runtime_error
         */
        bool ConfigFPGAFromJson(const std::string &json_filename);

        /**
         * @brief Indicate config is down.
         */
        bool ConfigDown() const;
    };
}

#endif
