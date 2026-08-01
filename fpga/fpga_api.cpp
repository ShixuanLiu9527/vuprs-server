#include "config.h"
#include "fpga/fpga_api.h"
#include "logger/check.h"

/* ----------------------------------------------------------------------------- */
/* ----------------------------- ADC Controller -------------------------------- */
/* ----------------------------------------------------------------------------- */

bool vuprs::FPGA_API__ADC__StartADC(vuprs::FPGAController *controller, double fs)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", "FPGA Controller not configured in advance.");

    uint32_t r_val, w_val;
    bool operate_status = true;

    /* STEP 1: Reset ADC */
    operate_status &= controller->dev__adc_controller.WriteSingleRegister(vuprs::ADC_Controller__Registers::ADC_RST, 0);
    /* STEP 2: Wait for stop */
    operate_status &= controller->dev__adc_controller.WaitForRegisterBIT(vuprs::ADC_Controller__Registers::ADC_STR, 0, 1, 100);
    /* STEP 3: Set sampling frequency fs */
    w_val = controller->dev__adc_controller.GetSCIValueForSamplingFrequency(fs);
    operate_status &= controller->dev__adc_controller.WriteSingleRegister(vuprs::ADC_Controller__Registers::ADC_SCI, w_val);
    /* STEP 4: Set continuous sampling */
    w_val = 0x00000001;
    operate_status &= controller->dev__adc_controller.WriteSingleRegister(vuprs::ADC_Controller__Registers::ADC_CS, w_val);
    /* STEP 5: Start sampling */
    operate_status &= controller->dev__adc_controller.WriteSingleRegister(vuprs::ADC_Controller__Registers::ADC_STR, 0);
    return operate_status;
}

bool vuprs::FPGA_API__ADC__ResetADC(vuprs::FPGAController *controller)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", "FPGA Controller not configured in advance.");

    bool operate_status = true;
    /* STEP 1: Reset ADC */
    operate_status &= controller->dev__adc_controller.WriteSingleRegister(vuprs::ADC_Controller__Registers::ADC_RST, 0);
    /* STEP 2: Wait for stop */
    operate_status &= controller->dev__adc_controller.WaitForRegisterBIT(vuprs::ADC_Controller__Registers::ADC_STR, 0, 1, 100);
    return operate_status;
}

/* ----------------------------------------------------------------------------- */
/* ----------------------------- Circular Buffer ------------------------------- */
/* ----------------------------------------------------------------------------- */

bool vuprs::FPGA_API__CBUF__ReadCircularBuffer(vuprs::FPGAController *controller,
                                               vuprs::SignalData *signal)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", "FPGA Controller not configured in advance.");
    RUNTIME_CHECK(controller->dev__circular_buffer.Refreshed(), "fpga", "Circular buffer not refreshed.");

    uint32_t r_val, w_val, CBF;
    bool operate_status = true;

    /* STEP 1: Clear buffer */
    vuprs::AlignedBufferDMA buffer;
    /* STEP 2: Freeze */
    operate_status &= controller->dev__circular_buffer.WriteSingleRegisterBIT(vuprs::Circular_Buffer__Registers::CBUF_FREEZE, 0, true);
    /* STEP 3: Wait for freezed */
    operate_status &= controller->dev__circular_buffer.WaitForRegisterBIT(vuprs::Circular_Buffer__Registers::CBUF_RS, 0, 1, 100);
    /* Read SCI, get fs & voltage */
    operate_status &= controller->dev__adc_controller.ReadSingleRegister(vuprs::ADC_Controller__Registers::ADC_SCI, &r_val);
    double fs = controller->dev__adc_controller.SCI2FS(r_val);
    double voltageScale = controller->dev__adc_controller.VoltageRangeRadius();
    /* STEP 3: Read circular buffer */
    uint32_t signal_points = controller->dev__circular_buffer.SignalPoints();
    operate_status &= controller->mem___circular_buffer_bram.ReadMemory(&buffer, 0, signal_points * ADC_FRAME_WORD_SIZE * sizeof(uint32_t));
    /* STEP 4: Read current BRAM pointer & convert */
    operate_status &= controller->dev__circular_buffer.ReadSingleRegister(vuprs::Circular_Buffer__Registers::CBUF_CBP, &CBF);
    uint32_t pointPos = std::max(0, FPGA_CBF_TO_DATA_POSITION(CBF)); /* rotate points = (CBF + 4) / 40 - 1 */
    operate_status &= vuprs::FPGACircularBuffer2Frames(&buffer, signal, fs, voltageScale, pointPos);
    /* STEP 5: Reset */
    operate_status &= controller->dev__circular_buffer.WriteSingleRegister(vuprs::Circular_Buffer__Registers::CBUF_RST, 0);
    return operate_status;
}

bool vuprs::FPGA_API__CBUF__ResetCircularBuffer(vuprs::FPGAController *controller)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", " in [vuprs::FPGA_API__CBUF__ResetCircularBuffer] FPGA Controller not configured in advance.");
    return controller->dev__circular_buffer.WriteSingleRegister(vuprs::Circular_Buffer__Registers::CBUF_RST, 0);
}

/* ----------------------------------------------------------------------------- */
/* ------------------------------ Predelay Unit -------------------------------- */
/* ----------------------------------------------------------------------------- */

bool vuprs::FPGA_API__PDLY__SetPredelay(vuprs::FPGAController *controller,
                                        const std::vector<int> &channel_predelay,
                                        const std::vector<std::string> &channel_name)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", " in [vuprs::FPGA_API__PDLY__SetPredelay] FPGA Controller not configured in advance.");
    PARAM_CHECK(channel_name.size() == channel_predelay.size(), "fpga", " in [vuprs::FPGA_API__PDLY__SetPredelay] Channel name list & channel predelay list not the same size.");
    PARAM_CHECK(channel_predelay.size() == ADC_CHANNEL_NUMBER, "fpga", " in [vuprs::FPGA_API__PDLY__SetPredelay] Invalid channel predelay size.");

    uint32_t r_val, w_val;
    std::vector<uint16_t> predelay_ordered(ADC_CHANNEL_NUMBER);
    std::vector<uint32_t> predelay_to_write(ADC_CHANNEL_NUMBER / 2);
    const std::vector<vuprs::PreDelay_Unit__Registers> registers_to_write = {
        vuprs::PreDelay_Unit__Registers::PREDLY_CH1_CH2,
        vuprs::PreDelay_Unit__Registers::PREDLY_CH3_CH4,
        vuprs::PreDelay_Unit__Registers::PREDLY_CH5_CH6,
        vuprs::PreDelay_Unit__Registers::PREDLY_CH7_CH8,
        vuprs::PreDelay_Unit__Registers::PREDLY_CH9_CH10,
        vuprs::PreDelay_Unit__Registers::PREDLY_CH11_CH12,
        vuprs::PreDelay_Unit__Registers::PREDLY_CH13_CH14,
        vuprs::PreDelay_Unit__Registers::PREDLY_CH15_CH16,
    };
    for (int i = 0; i < ADC_CHANNEL_NUMBER; i++)
    {
        int pos = vuprs::FindValueInVec<std::string>(channel_name, ADC_CHANNEL_ADDR_MAP[i]);
        PARAM_CHECK(pos >= 0, "fpga", " in [vuprs::FPGA_API__PDLY__SetPredelay] Missing channel: " + ADC_CHANNEL_ADDR_MAP[i]);
        predelay_ordered[i] = static_cast<uint16_t>(channel_predelay[pos]);
    }
    for (int i = 0; i < ADC_CHANNEL_NUMBER / 2; i++)
    {
        predelay_to_write[i] = UINT16_SPLI_TO_UINT32(predelay_ordered[2 * i], predelay_ordered[2 * i + 1]);
    }
    return controller->dev__predelay_unit.WriteMultipleRegister(registers_to_write, predelay_to_write);
}

bool vuprs::FPGA_API__PDLY__ResetPredelay(vuprs::FPGAController *controller)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", " in [vuprs::FPGA_API__PDLY__ResetPredelay] FPGA Controller not configured in advance.");
    return controller->dev__predelay_unit.WriteSingleRegister(vuprs::PreDelay_Unit__Registers::PREDLY_RST, 0);
}

/* ----------------------------------------------------------------------------- */
/* ---------------------------- FIR Filter Bank -------------------------------- */
/* ----------------------------------------------------------------------------- */

bool vuprs::FPGA_API__FIR__SetCoefficients(vuprs::FPGAController *controller,
                                           std::vector<std::vector<double>> *coefficients,
                                           double max_absolute_coefficient)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", " in [vuprs::FPGA_API__FIR__SetCoefficients] FPGA Controller not configured in advance.");
    PARAM_CHECK(!coefficients->empty(), "fpga", " in [vuprs::FPGA_API__FIR__SetCoefficients] Coefficients empty.");
#if DEBUG
    static int debug_file_group = 0;
#endif
    uint64_t banks = coefficients->size();
    int check_coefficients_count = -1;
    for (uint64_t i = 0; i < banks; i++)
    {
        PARAM_CHECK(!(*coefficients)[i].empty(), "fpga", " in [vuprs::FPGA_API__FIR__SetCoefficients] Coefficients bank [" + std::to_string(i) + "] empty.");
        if (check_coefficients_count < 0)
        {
            check_coefficients_count = (*coefficients)[i].size();
        }
        else
        {
            PARAM_CHECK((*coefficients)[i].size() == check_coefficients_count, "fpga", " in [vuprs::FPGA_API__FIR__SetCoefficients] Inconsistent length of coefficients.");
        }
    }
    uint32_t r_val;
    bool operate_status = true;
    /* Read FIR length */
    operate_status &= controller->dev__fir_filter_bank.ReadSingleRegister(vuprs::FIR_Filter_Bank__Registers::FIR_LEN, &r_val);
    PARAM_CHECK(r_val == static_cast<uint32_t>(check_coefficients_count), "fpga", " in [vuprs::FPGA_API__FIR__SetCoefficients] len(FIR) != len(coef[0])");
    vuprs::AlignedBufferDMA buffer;
    /* Clear buffer */
    std::vector<uint32_t> coefficients_to_write, one_bank_coefficients;
    uint32_t total_coefficients_count = 0;
    /* Convert double to Q31 uint32_t */
    for (uint64_t i = 0; i < banks; i++)
    {
        vuprs::FIRCoefficient_DOUBLE_TO_Q31_UINT32((*coefficients)[i], &one_bank_coefficients, max_absolute_coefficient);
        total_coefficients_count += one_bank_coefficients.size();
        coefficients_to_write.insert(coefficients_to_write.end(), one_bank_coefficients.begin(), one_bank_coefficients.end());
    }
    /* Data to buffer */
    buffer.from_vector<uint32_t>(coefficients_to_write);
#if DEBUG
    vuprs::SaveToCSV(*coefficients, std::string(DEBUG_FILES_ROOT_DIR) + "/" +
                                        std::string(DEBUG_FILES_DIR) + "-" + std::to_string(debug_file_group) + "/" +
                                        std::string(FIR_COEF_DEBUG_FILENAME));
    buffer.to_file(std::string(DEBUG_FILES_ROOT_DIR) + "/" +
                   std::string(DEBUG_FILES_DIR) + "-" + std::to_string(debug_file_group) + "/" +
                   std::string(FIR_COEF_BIN_DEBUG_FILENAME));
    debug_file_group++;
    if (debug_file_group >= DEBUG_DATA_GROUP_COUNT)
        debug_file_group = 0;
#endif
    /* Write coefficients to BRAM */
    operate_status &= controller->mem__fir_bram.WriteMemory(&buffer, 0, total_coefficients_count * sizeof(uint32_t));
    /* Write scale to FIR */
    double fir_scale_in_double = controller->dev__adc_controller.VoltageRangeRadius() * max_absolute_coefficient;
    uint32_t fir_scale_to_write = vuprs::Q16__DOUBLE_TO_UINT32(fir_scale_in_double);
    operate_status &= controller->dev__fir_filter_bank.WriteSingleRegister(vuprs::FIR_Filter_Bank__Registers::FIR_COEF_SCALE, fir_scale_to_write);
    /* Trigger coefficient update */
    operate_status &= controller->dev__fir_filter_bank.WriteSingleRegister(vuprs::FIR_Filter_Bank__Registers::FIR_U_FIR_COEF, 0);
    return operate_status;
}

bool vuprs::FPGA_API__FIR__SetLengthAndCoefficients(vuprs::FPGAController *controller,
                                                    std::vector<std::vector<double>> *coefficients,
                                                    double max_absolute_coefficient,
                                                    uint32_t len)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", " in [vuprs::FPGA_API__FIR__SetLengthAndCoefficients] FPGA Controller not configured in advance.");
    PARAM_CHECK(!coefficients->empty(), "fpga", " in [vuprs::FPGA_API__FIR__SetLengthAndCoefficients] Coefficients empty.");
    uint64_t banks = coefficients->size();
    int check_coefficients_count = -1;
    for (uint64_t i = 0; i < banks; i++)
    {
        PARAM_CHECK(!(*coefficients)[i].empty(), "fpga", " in [vuprs::FPGA_API__FIR__SetLengthAndCoefficients] Coefficients bank [" + std::to_string(i) + "] empty.");
        if (check_coefficients_count < 0)
        {
            check_coefficients_count = (*coefficients)[i].size();
        }
        else
        {
            PARAM_CHECK((*coefficients)[i].size() == check_coefficients_count, "fpga", " in [vuprs::FPGA_API__FIR__SetLengthAndCoefficients] Inconsistent length of coefficients.");
        }
    }
    PARAM_CHECK(len == static_cast<uint32_t>(check_coefficients_count), "fpga", " in [vuprs::FPGA_API__FIR__SetLengthAndCoefficients] len(FIR) != len(coef[0])");
    /* Check length valid */
    uint32_t r_val;
    bool operate_status = true;
    operate_status &= controller->dev__fir_filter_bank.ReadSingleRegister(vuprs::FIR_Filter_Bank__Registers::FIR_MAX_LEN, &r_val);
    PARAM_CHECK(len <= r_val, "fpga", " in [vuprs::FPGA_API__FIR__SetLengthAndCoefficients] Invalid FIR length (valid: <= " + std::to_string(r_val) + ").");
    /* Clear buffer */
    vuprs::AlignedBufferDMA buffer;
    buffer.release();
    std::vector<uint32_t> coefficients_to_write, one_bank_coefficients;
    uint32_t total_coefficients_count = 0;
    /* Convert double to Q31 uint32_t */
    for (uint64_t i = 0; i < banks; i++)
    {
        vuprs::FIRCoefficient_DOUBLE_TO_Q31_UINT32((*coefficients)[i], &one_bank_coefficients, max_absolute_coefficient);
        total_coefficients_count += one_bank_coefficients.size();
        coefficients_to_write.insert(coefficients_to_write.end(), one_bank_coefficients.begin(), one_bank_coefficients.end());
    }
    /* Read data from vector to buffer */
    buffer.from_vector<uint32_t>(coefficients_to_write);
    /* Write coefficients to BRAM */
    operate_status &= controller->mem__fir_bram.WriteMemory(&buffer, 0, total_coefficients_count * sizeof(uint32_t));
    /* Write length to FIR */
    operate_status &= controller->dev__fir_filter_bank.WriteSingleRegister(vuprs::FIR_Filter_Bank__Registers::FIR_LEN, len);
    /* Write scale to FIR */
    double fir_scale_in_double = controller->dev__adc_controller.VoltageRangeRadius() * max_absolute_coefficient;
    uint32_t fir_scale_to_write = vuprs::Q16__DOUBLE_TO_UINT32(fir_scale_in_double);
    operate_status &= controller->dev__fir_filter_bank.WriteSingleRegister(vuprs::FIR_Filter_Bank__Registers::FIR_COEF_SCALE, fir_scale_to_write);
    /* Trigger length update */
    operate_status &= controller->dev__fir_filter_bank.WriteSingleRegister(vuprs::FIR_Filter_Bank__Registers::FIR_U_FIR_LEN, 0);
    return operate_status;
}

bool vuprs::FPGA_API__FIR__ResetFIR(vuprs::FPGAController *controller)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", " in [vuprs::FPGA_API__FIR__ResetFIR] FPGA Controller not configured in advance.");
    bool operate_status = true;
    operate_status &= controller->dev__fir_filter_bank.WriteSingleRegister(vuprs::FIR_Filter_Bank__Registers::FIR_RST, 0);
    operate_status &= controller->dev__fir_filter_bank.WriteSingleRegisterBIT(vuprs::FIR_Filter_Bank__Registers::FIR_RSC, 0, false);
    return operate_status;
}

bool vuprs::FPGA_API__FIR__RunningControl(vuprs::FPGAController *controller, bool run_enable)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", " in [vuprs::FPGA_API__FIR__RunningControl] FPGA Controller not configured in advance.");
    bool operate_status = true;
    operate_status &= controller->dev__fir_filter_bank.WriteSingleRegisterBIT(vuprs::FIR_Filter_Bank__Registers::FIR_RSC, 0, run_enable);
    return operate_status;
}

/* ----------------------------------------------------------------------------- */
/* ------------------------------------ DDR ------------------------------------ */
/* ----------------------------------------------------------------------------- */

bool vuprs::FPGA_API__DDR__ReadDDR(vuprs::FPGAController *controller,
                                   vuprs::AlignedBufferDMA *buffer,
                                   uint32_t ddr_offset,
                                   uint32_t transfer_size)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", " in [vuprs::FPGA_API__DDR__ReadDDR] FPGA Controller not configured in advance.");
    return controller->mem__ddr.ReadMemory(buffer, ddr_offset, transfer_size);
}

/* ----------------------------------------------------------------------------- */
/* ---------------------------------- AXI DMA ---------------------------------- */
/* ----------------------------------------------------------------------------- */

bool vuprs::FPGA_API__DMA__StartScatterGatherDMA_S2MM(vuprs::FPGAController *controller,
                                                      const std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> &descriptors,
                                                      bool is_cyclic_mode,
                                                      bool enable_ioc_interrupt)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", " in [vuprs::FPGA_API__DMA__StartScatterGatherDMA_S2MM] FPGA Controller not configured in advance.");
    uint32_t descriptorSize = descriptors.size();
    PARAM_CHECK(descriptorSize > 0, "fpga", " in [vuprs::FPGA_API__DMA__StartScatterGatherDMA_S2MM] Descriptor is empty.");
    PARAM_CHECK(!(is_cyclic_mode && descriptors[descriptorSize - 1].NXTDESC != descriptors[0].ALIGNMENT_0_CURRENT_ADDR), "fpga", " in [vuprs::FPGA_API__DMA__StartScatterGatherDMA_S2MM] Invalid cyclic DMA descriptor.");

    uint32_t r_val, w_val;
    bool operate_status = true;
    /* Clear buffer & reset DMA */
    vuprs::AlignedBufferDMA buffer;
    buffer.release();
    vuprs::FPGA_API__DMA__ResetDMA(controller);
    /* STEP 1: Stop DMA, clear S2MM_DMACR.RS */
    operate_status &= controller->dev__axi_dma.WriteSingleRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMACR, 0, false);
    /* Wait for S2MM_DMASR.Halted = 1 */
    operate_status &= controller->dev__axi_dma.WaitForRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMASR, 0, 1, 100);
    /* STEP 2: Write descriptor address to current descriptor pointer */
    operate_status &= controller->dev__axi_dma.WriteSingleRegister(vuprs::AXI_DMA__Registers::S2MM_CURDESC, descriptors[0].ALIGNMENT_0_CURRENT_ADDR);
    /* (STEP 2): Enable IOC interrupt */
    if (enable_ioc_interrupt)
    {
        /* S2MM_DMACR.IOC_IRqEn = 1 */
        operate_status &= controller->dev__axi_dma.WriteSingleRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMACR, 12, true);
        /* S2MM_DMACR.IRQThreshold = 0x01 */
        operate_status &= controller->dev__axi_dma.WriteSingleRegisterBITRegion(vuprs::AXI_DMA__Registers::S2MM_DMACR, 16, 23, 0x01);
        /* S2MM_DMACR.IRQDelay = 0 */
        operate_status &= controller->dev__axi_dma.WriteSingleRegisterBITRegion(vuprs::AXI_DMA__Registers::S2MM_DMACR, 24, 31, 0);
    }
    /* (STEP 2): Set Cyclic BD Enable */
    if (is_cyclic_mode) /* Set bit: S2MM_DMACR.[4] */
    {
        operate_status &= controller->dev__axi_dma.WriteSingleRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMACR, 4, true);
    }
    /* STEP 3: Start DMA, set S2MM_DMACR.RS = 1 */
    operate_status &= controller->dev__axi_dma.WriteSingleRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMACR, 0, true);
    /* Wait for S2MM_DMASR.Halted = 0 */
    operate_status &= controller->dev__axi_dma.WaitForRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMASR, 0, 0, 100);
    /* STEP 4: Write descriptors to SG_BRAM */
    buffer.from_vector<vuprs::AXI_DMA_ScatterGatherDescriptor>(descriptors);
    operate_status &= controller->mem__sg_bram.WriteMemory(&buffer, 0x00, buffer.size());
    /* STEP 5: Write tail descriptor register to trigger. */
    if (is_cyclic_mode)
    {
        w_val = (uint32_t)((uint32_t)0x50 << 6); /* Write to [31:6] */
    }
    else
    {
        w_val = (uint32_t)((uint32_t)(descriptors[descriptorSize - 1].ALIGNMENT_0_CURRENT_ADDR) << 6); /* Write to [31:6] */
    }
    operate_status &= controller->dev__axi_dma.WriteSingleRegister(vuprs::AXI_DMA__Registers::S2MM_TAILDESC, w_val);
    return operate_status;
}

bool vuprs::FPGA_API__DMA__GetAndClearInterruptFlag(vuprs::FPGAController *controller, uint32_t *flag)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", " in [vuprs::FPGA_API__DMA__GetAndClearInterruptFlag] FPGA Controller not configured in advance.");
    PARAM_CHECK(flag != nullptr, "fpga", " in [vuprs::FPGA_API__DMA__GetAndClearInterruptFlag] FLAG is NULL.");

    uint32_t r_val;
    bool operate_status = true;
    /* Read device to detect interrupt */
    operate_status &= controller->dev__axi_dma.ReadEvent(&r_val);
    *flag = r_val;
    if (r_val == 0) /* no interrupt */
    {
        return operate_status;
    }
    /* Clear flags (write 1 to S2MM_DMASR.IOC_Irq) */
    operate_status &= controller->dev__axi_dma.WriteSingleRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMASR, 12, true);
    return operate_status;
}

bool vuprs::FPGA_API__DMA__ResetDMA(vuprs::FPGAController *controller)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", " in [vuprs::FPGA_API__DMA__ResetDMA] FPGA Controller not configured in advance.");

    bool operate_status = true;
    /* Reset */
    operate_status &= controller->dev__axi_dma.WriteSingleRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMACR, 2, true);
    /* S2MM_DMACR.IOC_IRqEn = 0 */
    operate_status &= controller->dev__axi_dma.WriteSingleRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMACR, 12, false);
    /* Wait for Halted */
    operate_status &= controller->dev__axi_dma.WaitForRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMASR, 0, 1, 100);
    return operate_status;
}

bool vuprs::FPGA_API__DMA__GetCurrentDescriptor(vuprs::FPGAController *controller,
                                                const std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> &reference_descriptors,
                                                vuprs::AXI_DMA_ScatterGatherDescriptor *current_descriptor,
                                                vuprs::AXI_DMA_ScatterGatherDescriptor *previous_descriptor,
                                                vuprs::AXI_DMA_ScatterGatherDescriptor *next_descriptor)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", " in [vuprs::FPGA_API__DMA__GetCurrentDescriptor] FPGA Controller not configured in advance.");

    bool operate_status = true, found = false;
    uint32_t r_val = INVALID_SG_DESCRIPTOR_POINTER + 1;
    uint32_t next_addr, previous_addr;
    operate_status &= vuprs::FPGA_API__DMA__ReadCurrentDescriptor(controller, &r_val);
    /* Match */
    operate_status &= vuprs::MatchDescriptor(reference_descriptors,
                                             r_val,
                                             current_descriptor,
                                             next_descriptor,
                                             previous_descriptor);
    RUNTIME_CHECK(found, "fpga", " in [vuprs::FPGA_API__DMA__GetCurrentDescriptor] Cannot found current descriptor with address: " + std::to_string(r_val));
    return operate_status;
}

bool vuprs::FPGA_API__DMA__ReadCurrentDescriptor(vuprs::FPGAController *controller,
                                                 uint32_t *current_descriptor)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", " in [vuprs::FPGA_API__DMA__ReadCurrentDescriptor] FPGA Controller not configured in advance.");
    PARAM_CHECK(current_descriptor != nullptr, "fpga", " in [vuprs::FPGA_API__DMA__ReadCurrentDescriptor] CURRENT_DESCRIPTOR is NULL.");
    bool operate_status = true;
    uint32_t r_val;
    operate_status &= controller->dev__axi_dma.ReadSingleRegister(vuprs::AXI_DMA__Registers::S2MM_CURDESC, &r_val);
    *current_descriptor = r_val;
    return operate_status;
}

bool vuprs::FPGA_API__DMA__SetTimeoutForInterrupt(vuprs::FPGAController *controller,
                                                  uint32_t timeout_ms)
{
    PARAM_CHECK(controller->ConfigDone(), "fpga", " in [vuprs::FPGA_API__DMA__SetTimeoutForInterrupt] FPGA Controller not configured in advance.");
    controller->dev__axi_dma.SetInterruptTimeout(timeout_ms);
    return true;
}
