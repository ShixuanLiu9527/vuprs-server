#ifndef FPGA_OPERATION_API_H
#define FPGA_OPERATION_API_H

#include "fpga/fpga_controller.h"
#include "fpga/fpga_data_conversion.h"

namespace vuprs
{
    /* ----------------------------------------------------------------------------- */
    /* ----------------------------- ADC Controller -------------------------------- */
    /* ----------------------------------------------------------------------------- */

    /**
     * @brief Start ADC sampling.
     *
     * @note Controller must be configured in advance.
     * @note Sampling frequency must smaller than MAX_FS
     * @note It is recommended to start at the very last.
     *
     * @param controller FPGA controller.
     * @param fs sampling frequency (unit: Hz).
     *
     * @retval true: success.
     * @retval false: failed.
     *
     * @throw std::runtime_error
     */
    bool FPGA_API__ADC__StartADC(vuprs::FPGAController *controller, double fs);

    /**
     * @brief Reset ADC.
     */
    bool FPGA_API__ADC__ResetADC(vuprs::FPGAController *controller);

    /* ----------------------------------------------------------------------------- */
    /* ----------------------------- Circular Buffer ------------------------------- */
    /* ----------------------------------------------------------------------------- */

    /**
     * @brief Read circular buffer.
     *
     * @note Controller must be configured in advance.
     * @note The function will read and reset safely.
     *
     * @param controller FPGA controller.
     * @param signal output signal data in circular buffer.
     *
     * @retval true: success.
     * @retval false: failed.
     *
     * @throw std::runtime_error.
     */
    bool FPGA_API__CBUF__ReadCircularBuffer(vuprs::FPGAController *controller,
                                            vuprs::SignalData *signal);

    /**
     * @brief Reset Circular Buffer.
     */
    bool FPGA_API__CBUF__ResetCircularBuffer(vuprs::FPGAController *controller);

    /* ----------------------------------------------------------------------------- */
    /* ------------------------------ Predelay Unit -------------------------------- */
    /* ----------------------------------------------------------------------------- */

    /**
     * @brief Set predelay for Pre-delay Unit.
     *
     * @note Controller must be configured in advance.
     * @note All predelay values should be equal or smaller than MAX_PDLY.
     *
     * @param controller FPGA controller.
     * @param channel_predelay channel predelay values (n * Ts).
     * @param channel_name corrsponding channel name.
     *
     * @retval true: success.
     * @retval false: failed.
     *
     * @throw std::runtime_error
     */
    bool FPGA_API__PDLY__SetPredelay(vuprs::FPGAController *controller,
                                     const std::vector<int> &channel_predelay,
                                     const std::vector<std::string> &channel_name);

    /**
     * @brief Reset Predelay Unit.
     */
    bool FPGA_API__PDLY__ResetPredelay(vuprs::FPGAController *controller);

    /* ----------------------------------------------------------------------------- */
    /* ---------------------------- FIR Filter Bank -------------------------------- */
    /* ----------------------------------------------------------------------------- */

    /**
     * @brief Set new FIR filter bank coefficients.
     *
     * @note Controller must be configured in advance.
     * @note coefficients[i].size() must equal to FIR_LEN.
     *
     * @param controller FPGA controller.
     * @param coefficients new coefficients of FIR filter bank (raw data, no need for scale).
     * @param max_absolute_coefficient max absolute coefficients (= max(abs(coefficients))).
     *
     * @retval true: success.
     * @retval false: failed.
     *
     * @throw std::runtime_error
     */
    bool FPGA_API__FIR__SetCoefficients(vuprs::FPGAController *controller,
                                        std::vector<std::vector<double>> *coefficients,
                                        double max_absolute_coefficient);

    /**
     * @brief Set new FIR filter bank coefficients & FIR filter length.
     *
     * @note Controller must be configured in advance.
     * @note coefficients[i].size() must equal to len.
     *
     * @param controller FPGA controller.
     * @param coefficients new coefficients of FIR filter bank (raw data, no need for scale).
     * @param max_absolute_coefficient max absolute coefficients (= max(abs(coefficients))).
     * @param len new length of FIR filter (<= MAX_FIR_LEN).
     *
     * @retval true: success.
     * @retval false: failed.
     *
     * @throw std::runtime_error
     */
    bool FPGA_API__FIR__SetLengthAndCoefficients(vuprs::FPGAController *controller,
                                                 std::vector<std::vector<double>> *coefficients,
                                                 double max_absolute_coefficient,
                                                 uint32_t len);

    /**
     * @brief Reset FIR filter bank.
     *
     * @note Controller must be configured in advance.
     * @note Step 1: Reset FIR.
     * @note Step 2: Disable run.
     */
    bool FPGA_API__FIR__ResetFIR(vuprs::FPGAController *controller);

    /**
     * @brief Enable/Disable FIR filter run.
     *
     * @note Controller must be configured in advance.
     *
     * @param controller FPGA controller.
     * @param run_enable true: enable run, false: disable run.
     */
    bool FPGA_API__FIR__RunningControl(vuprs::FPGAController *controller, bool run_enable);

    /* ----------------------------------------------------------------------------- */
    /* ------------------------------------ DDR ------------------------------------ */
    /* ----------------------------------------------------------------------------- */

    /**
     * @brief Read data from DDR to buffer.
     *
     * @note Controller must be configured in advance.
     *
     * @param controller FPGA controller.
     * @param buffer aligned buffer.
     * @param ddr_offset address offset in DDR (where to start reading).
     * @param transfer_size transfer size in bytes.
     *
     * @retval true: success.
     * @retval false: failed.
     *
     * @throw std::runtime_error
     */
    bool FPGA_API__DDR__ReadDDR(vuprs::FPGAController *controller,
                                vuprs::AlignedBufferDMA *buffer,
                                uint32_t ddr_offset,
                                uint32_t transfer_size);

    /* ----------------------------------------------------------------------------- */
    /* ---------------------------------- AXI DMA ---------------------------------- */
    /* ----------------------------------------------------------------------------- */

    /**
     * @brief Start Scatter/Gather Transfer.
     *
     * @note Controller must be configured in advance.
     * @note A DMA operation for the S2MM channel is set up and started by using the following sequence:
     * @note 1. Write the address of the starting descriptor to the Current Descriptor register.
     *          If AXI DMA is configured for an address space greater than 32,
     *          then also program the MSB 32 bits of the current descriptor.
     * @note 2. Start the S2MM channel running by setting the run/stop bit to 1 (S2MM_DMACR.RS =1).
     *          The halted bit (DMASR.Halted) should deassert indicating the S2MM channel is running.
     * @note 3. If desired, enable interrupts by writing a 1 to S2MM_DMACR.IOC_IrqEn and
     *          S2MM_DMACR.Err_IrqEn.
     * @note 4. Write a valid address to the Tail Descriptor register.
     *          If AXI DMA is configured for an address space greater than 32,
     *          then also program the MSB 32 bits of the current descriptor.
     * @note 5. Writing to the Tail Descriptor register triggers the DMA to start
     *          fetching the descriptors from the memory.
     * @note 6. The fetched descriptors are processed and any data received from
     *          the S2MM streaming channel is written to the memory.
     * @note For Cyclic DMA Mode: Program the Tail Descriptor register with some value
     *       which is not a part of the BD chain. Say for example 0x50.
     * @note For Cyclic DMA Mode: Ensure that the cyclic bit in the control register is set (S2MM_DMACR.[4] = 1).
     *
     * @param controller FPGA controller.
     * @param descriptors AXI DMA Scatter/Gather Descriptor list.
     * @param is_cyclic_mode true: Cyclic DMA Mode, false: Normal Mode.
     * @param enable_ioc_interrupt true: enable IOC Interrupt, false: disable IOC Interrupt.
     *
     * @retval true: success.
     * @retval false: failed.
     *
     * @throw std::runtime_error
     */
    bool FPGA_API__DMA__StartScatterGatherDMA_S2MM(vuprs::FPGAController *controller,
                                                   const std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> &descriptors,
                                                   bool is_cyclic_mode,
                                                   bool enable_ioc_interrupt);

    /**
     * @brief Get IOC interrupt flag of AXI DMA (in Scatter/Gather mode).
     *
     * @note Controller must be configured in advance.
     * @note Make sure that the IOC interrupt is enabled.
     * @note If IOC interrupt detected, this API function will clear IOC interrupt flag in AXI DMA.
     *
     * @param controller FPGA controller.
     * @param flag 1: IOC Interrupt detected, 0: No IOC Interrupt.
     *
     * @retval true: success.
     * @retval false: failed.
     *
     * @throw std::runtime_error
     */
    bool FPGA_API__DMA__GetAndClearInterruptFlag(vuprs::FPGAController *controller, uint32_t *flag);

    /**
     * @brief Reset AXI DMA.
     *
     * @note Controller must be configured in advance.
     * @note Interrupt will be disabled.
     */
    bool FPGA_API__DMA__ResetDMA(vuprs::FPGAController *controller);

    /**
     * @brief Get current descriptor.
     *
     * @note Controller must be configured in advance.
     *
     * @param controller FPGA controller.
     * @param reference_descriptors reference descriptors (to match).
     * @param current_descriptor current descriptor.
     * @param previous_descriptor previous descriptor.
     * @param next_descriptor next descriptor.
     *
     * @retval true: success.
     * @retval false: failed.
     *
     * @throw std::runtime_error
     */
    bool FPGA_API__DMA__GetCurrentDescriptor(vuprs::FPGAController *controller,
                                             const std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> &reference_descriptors,
                                             vuprs::AXI_DMA_ScatterGatherDescriptor *current_descriptor,
                                             vuprs::AXI_DMA_ScatterGatherDescriptor *previous_descriptor,
                                             vuprs::AXI_DMA_ScatterGatherDescriptor *next_descriptor);

    /**
     * @brief Read current descriptor address.
     *
     * @note Controller must be configured in advance.
     * @note The API function will read the current descriptor address from AXI DMA register,
     *
     * @param controller FPGA controller.
     * @param current_descriptor current descriptor address.
     *
     * @retval true: success.
     * @retval false: failed.
     */
    bool FPGA_API__DMA__ReadCurrentDescriptor(vuprs::FPGAController *controller, uint32_t *current_descriptor);

    /**
     * @brief Set timeout for interrupt detection in AXI DMA.
     *
     * @param controller FPGA controller.
     * @param timeout_ms timeout in milliseconds.
     *
     * @retval true: success.
     * @retval false: failed.
     *
     * @throw std::runtime_error
     */
    bool FPGA_API__DMA__SetTimeoutForInterrupt(vuprs::FPGAController *controller, uint32_t timeout_ms);
}

#endif
