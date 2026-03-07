#include "fpga_api.h"

/* ----------------------------------------------------------------------------- */
/* ----------------------------- ADC Controller -------------------------------- */
/* ----------------------------------------------------------------------------- */

bool vuprs::FPGA_API__ADC__StartADC(vuprs::FPGAController *controller, double fs)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }

    uint32_t r_val, w_val;
    bool operateStatus = true;

    /* STEP 1: Reset ADC */

    operateStatus &= controller->dev__ADC_Controller.WriteSingleRegister(vuprs::ADC_Controller__Registers::ADC_RST, 0);

    /* STEP 2: Wait for stop */

    operateStatus &= controller->dev__ADC_Controller.WaitForRegisterBIT(vuprs::ADC_Controller__Registers::ADC_STR, 0, 1, 100);

    /* STEP 3: Set sampling frequency fs */

    w_val = controller->dev__ADC_Controller.GetSCIValueForSamplingFrequency(fs);
    operateStatus &= controller->dev__ADC_Controller.WriteSingleRegister(vuprs::ADC_Controller__Registers::ADC_SCI, w_val);
    
    /* STEP 4: Set continuous sampling */

    w_val = 0x00000001;
    operateStatus &= controller->dev__ADC_Controller.WriteSingleRegister(vuprs::ADC_Controller__Registers::ADC_CS, w_val);

    /* STEP 5: Start sampling */

    operateStatus &= controller->dev__ADC_Controller.WriteSingleRegister(vuprs::ADC_Controller__Registers::ADC_STR, 0);

    return operateStatus;
}

bool vuprs::FPGA_API__ADC__ResetADC(vuprs::FPGAController *controller)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }

    bool operateStatus = true;

    /* STEP 1: Reset ADC */

    operateStatus &= controller->dev__ADC_Controller.WriteSingleRegister(vuprs::ADC_Controller__Registers::ADC_RST, 0);

    /* STEP 2: Wait for stop */

    operateStatus &= controller->dev__ADC_Controller.WaitForRegisterBIT(vuprs::ADC_Controller__Registers::ADC_STR, 0, 1, 100);

    return operateStatus;
}

/* ----------------------------------------------------------------------------- */
/* ----------------------------- Circular Buffer ------------------------------- */
/* ----------------------------------------------------------------------------- */

bool vuprs::FPGA_API__CBUF__ReadCircularBuffer(vuprs::FPGAController *controller, 
    vuprs::SignalData *signal)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }
    if (!controller->dev__Circular_Buffer.Refreshed())
    {
        throw std::runtime_error("Circular buffer not refreshed.");
    }

    uint32_t r_val, w_val, CBF;
    bool operateStatus = true;

    /* STEP 1: Clear buffer */

    vuprs::AlignedBufferDMA buffer;

    /* STEP 2: Freeze */

    operateStatus &= controller->dev__Circular_Buffer.WriteSingleRegister(vuprs::Circular_Buffer__Registers::CBUF_FREEZE, 0x00000001);

    /* STEP 3: Wait for freezed */

    operateStatus &= controller->dev__Circular_Buffer.WaitForRegisterBIT(vuprs::Circular_Buffer__Registers::CBUF_RS, 0, 1, 100);

    /* Read SCI, get fs & voltage */

    operateStatus &= controller->dev__ADC_Controller.ReadSingleRegister(vuprs::ADC_Controller__Registers::ADC_SCI, &r_val);

    double fs = controller->dev__ADC_Controller.SCI2FS(r_val);
    double voltageScale = controller->dev__ADC_Controller.VoltageRangeRadius();

    /* STEP 3: Read circular buffer */

    uint32_t signalPoints = controller->dev__Circular_Buffer.SignalPoints();
    operateStatus &= controller->mem__Circular_Buffer_BRAM.ReadMemory(&buffer, 0, signalPoints * ADC_FRAME_WORD_SIZE * sizeof(uint32_t));

    /* STEP 4: Read current BRAM pointer & convert */

    operateStatus &= controller->dev__Circular_Buffer.ReadSingleRegister(vuprs::Circular_Buffer__Registers::CBUF_CBP, &CBF);
    operateStatus &= vuprs::FPGACircularBuffer2Frames(&buffer, signal, fs, voltageScale, CBF);

    /* STEP 5: Reset */

    operateStatus &= controller->dev__Circular_Buffer.WriteSingleRegister(vuprs::Circular_Buffer__Registers::CBUF_RST, 0);

    return operateStatus;
}

bool vuprs::FPGA_API__CBUF__ResetCircularBuffer(vuprs::FPGAController *controller)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }

    return controller->dev__Circular_Buffer.WriteSingleRegister(vuprs::Circular_Buffer__Registers::CBUF_RST, 0);
}

/* ----------------------------------------------------------------------------- */
/* ------------------------------ Predelay Unit -------------------------------- */
/* ----------------------------------------------------------------------------- */

bool vuprs::FPGA_API__PDLY__SetPredelay(vuprs::FPGAController *controller, 
    const std::vector<int> &channelPredelay, const std::vector<std::string> &channelName)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }
    if (channelName.size() != channelPredelay.size())
    {
        throw std::runtime_error("Channel name list & channel predelay list not the same size.");
    }
    if (channelPredelay.size() != ADC_CHANNEL_NUMBER)
    {
        throw std::runtime_error("Invalid channel predelay size.");
    }
    
    uint32_t r_val, w_val;
    
    std::vector<uint16_t> predelayOrdered(ADC_CHANNEL_NUMBER);
    std::vector<uint32_t> predelayToWrite(ADC_CHANNEL_NUMBER / 2);
    const std::vector<vuprs::PreDelay_Unit__Registers> registersToWrite = {
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
        int pos = vuprs::FindValueInVec<std::string>(channelName, ADC_CHANNEL_ADDR_MAP[i]);
        if (pos >= 0)
        {
            predelayOrdered[i] = static_cast<uint16_t>(channelPredelay[pos]);
        }
        else
        {
            throw std::runtime_error("Missing channel: " + ADC_CHANNEL_ADDR_MAP[i]);
        }
    }
    for (int i = 0; i < ADC_CHANNEL_NUMBER / 2; i++)
    {
        predelayToWrite[i] = UINT16_SPLI_TO_UINT32(predelayOrdered[2*i], predelayOrdered[2*i+1]);
    }

    return controller->dev__PreDelay_Unit.WriteMultipleRegister(registersToWrite, predelayToWrite);
}

bool vuprs::FPGA_API__PDLY__ResetPredelay(vuprs::FPGAController *controller)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }

    return controller->dev__PreDelay_Unit.WriteSingleRegister(vuprs::PreDelay_Unit__Registers::PREDLY_RST, 0);
}

/* ----------------------------------------------------------------------------- */
/* ---------------------------- FIR Filter Bank -------------------------------- */
/* ----------------------------------------------------------------------------- */

bool vuprs::FPGA_API__FIR__SetCoefficients(vuprs::FPGAController *controller, 
    std::vector<std::vector<double>> *coefficients, double maxAbsoluteCoefficient)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }
    if (coefficients->empty())
    {
        throw std::runtime_error("Coefficients empty.");
    }

    uint64_t banks = coefficients->size();
    int checkCoefficientsCount = -1;

    for (uint64_t i = 0; i < banks; i++)
    {
        if ((*coefficients)[i].empty())
        {
            throw std::runtime_error("Coefficients bank [" + std::to_string(i) + "] empty.");
        }
        if (checkCoefficientsCount < 0) 
        {
            checkCoefficientsCount = (*coefficients)[i].size();
        }
        else
        {
            if ((*coefficients)[i].size() != checkCoefficientsCount)
            {
                throw std::runtime_error("Inconsistent length of coefficients.");
            }
        }
    }

    uint32_t r_val;
    bool operateStatus = true;

    /* Read FIR length */

    operateStatus &= controller->dev__FIR_Filter_Bank.ReadSingleRegister(vuprs::FIR_Filter_Bank__Registers::FIR_LEN, &r_val);

    if (r_val != static_cast<uint32_t>(checkCoefficientsCount))
    {
        throw std::runtime_error("len(FIR) != len(coef[0])");
    }

    vuprs::AlignedBufferDMA buffer;

    /* Clear buffer */

    buffer.release();

    std::vector<uint32_t> coefficientsToWrite, oneBankCoefficients;
    uint32_t totalCoefficientsCount = 0;

    /* Convert double to Q31 uint32_t */

    for (uint64_t i = 0; i < banks; i++)
    {
        vuprs::FIRCoefficient_DOUBLE_TO_Q31_UINT32((*coefficients)[i], &oneBankCoefficients, maxAbsoluteCoefficient);
        totalCoefficientsCount += oneBankCoefficients.size();
        coefficientsToWrite.insert(coefficientsToWrite.end(), oneBankCoefficients.begin(), oneBankCoefficients.end());
    }

    /* Data to buffer */

    buffer.from_vector<uint32_t>(coefficientsToWrite);

    /* Write coefficients to BRAM */

    operateStatus &= controller->mem__FIR_BRAM.WriteMemory(&buffer, 0, totalCoefficientsCount * sizeof(uint32_t));

    /* Write scale to FIR */

    double firScaleInDouble = controller->dev__ADC_Controller.VoltageRangeRadius() * maxAbsoluteCoefficient;
    uint32_t firScaleToWrite = vuprs::Q16__DOUBLE_TO_UINT32(firScaleInDouble);

    operateStatus &= controller->dev__FIR_Filter_Bank.WriteSingleRegister(vuprs::FIR_Filter_Bank__Registers::FIR_COEF_SCALE, firScaleToWrite);

    /* Trigger coefficient update */

    operateStatus &= controller->dev__FIR_Filter_Bank.WriteSingleRegister(vuprs::FIR_Filter_Bank__Registers::FIR_U_FIR_COEF, 0);

    return operateStatus;
}

bool vuprs::FPGA_API__FIR__SetLengthAndCoefficients(vuprs::FPGAController *controller, 
    std::vector<std::vector<double>> *coefficients, double maxAbsoluteCoefficient, uint32_t len)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }
    if (coefficients->empty())
    {
        throw std::runtime_error("Coefficients empty.");
    }

    uint64_t banks = coefficients->size();
    int checkCoefficientsCount = -1;

    for (uint64_t i = 0; i < banks; i++)
    {
        if ((*coefficients)[i].empty())
        {
            throw std::runtime_error("Coefficients bank [" + std::to_string(i) + "] empty.");
        }
        if (checkCoefficientsCount < 0) 
        {
            checkCoefficientsCount = (*coefficients)[i].size();
        }
        else
        {
            if ((*coefficients)[i].size() != checkCoefficientsCount)
            {
                throw std::runtime_error("Inconsistent length of coefficients.");
            }
        }
    }

    if (len != static_cast<uint32_t>(checkCoefficientsCount))
    {
        throw std::runtime_error("len(FIR) != len(coef[0])");
    }

    /* Check length valid */

    uint32_t r_val;
    bool operateStatus = true;

    operateStatus &= controller->dev__FIR_Filter_Bank.ReadSingleRegister(vuprs::FIR_Filter_Bank__Registers::FIR_MAX_LEN, &r_val);

    if (len > r_val)
    {
        throw std::runtime_error("Invalid FIR length (valid: <= " + std::to_string(r_val) + ").");
    }

    /* Clear buffer */

    vuprs::AlignedBufferDMA buffer;
    buffer.release();

    std::vector<uint32_t> coefficientsToWrite, oneBankCoefficients;
    uint32_t totalCoefficientsCount = 0;

    /* Convert double to Q31 uint32_t */

    for (uint64_t i = 0; i < banks; i++)
    {
        vuprs::FIRCoefficient_DOUBLE_TO_Q31_UINT32((*coefficients)[i], &oneBankCoefficients, maxAbsoluteCoefficient);
        totalCoefficientsCount += oneBankCoefficients.size();
        coefficientsToWrite.insert(coefficientsToWrite.end(), oneBankCoefficients.begin(), oneBankCoefficients.end());
    }

    /* Read data from vector to buffer */

    buffer.from_vector<uint32_t>(coefficientsToWrite);

    /* Write coefficients to BRAM */

    operateStatus &= controller->mem__FIR_BRAM.WriteMemory(&buffer, 0, totalCoefficientsCount * sizeof(uint32_t));

    /* Write length to FIR */

    operateStatus &= controller->dev__FIR_Filter_Bank.WriteSingleRegister(vuprs::FIR_Filter_Bank__Registers::FIR_LEN, len);

    /* Write scale to FIR */

    double firScaleInDouble = controller->dev__ADC_Controller.VoltageRangeRadius() * maxAbsoluteCoefficient;
    uint32_t firScaleToWrite = vuprs::Q16__DOUBLE_TO_UINT32(firScaleInDouble);

    operateStatus &= controller->dev__FIR_Filter_Bank.WriteSingleRegister(vuprs::FIR_Filter_Bank__Registers::FIR_COEF_SCALE, firScaleToWrite);

    /* Trigger length update */

    operateStatus &= controller->dev__FIR_Filter_Bank.WriteSingleRegister(vuprs::FIR_Filter_Bank__Registers::FIR_U_FIR_LEN, 0);

    return operateStatus;
}

bool vuprs::FPGA_API__FIR__ResetFIR(vuprs::FPGAController *controller)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }

    bool operateStatus = true;

    operateStatus &= controller->dev__FIR_Filter_Bank.WriteSingleRegister(vuprs::FIR_Filter_Bank__Registers::FIR_RST, 0);
    operateStatus &= controller->dev__FIR_Filter_Bank.WriteSingleRegisterBIT(vuprs::FIR_Filter_Bank__Registers::FIR_RSC, 0, false);

    return operateStatus;
}

bool vuprs::FPGA_API__FIR__RuningControl(vuprs::FPGAController *controller, bool runEnable)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }

    bool operateStatus = true;

    operateStatus &= controller->dev__FIR_Filter_Bank.WriteSingleRegisterBIT(vuprs::FIR_Filter_Bank__Registers::FIR_RSC, 0, runEnable);

    return operateStatus;
}

/* ----------------------------------------------------------------------------- */
/* ------------------------------------ DDR ------------------------------------ */
/* ----------------------------------------------------------------------------- */

bool vuprs::FPGA_API__DDR__ReadDDR(vuprs::FPGAController *controller, 
    vuprs::AlignedBufferDMA *buffer, uint32_t ddrOffset, uint32_t transferSize)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }

    return controller->mem__DDR.ReadMemory(buffer, ddrOffset, transferSize);
}

/* ----------------------------------------------------------------------------- */
/* ---------------------------------- AXI DMA ---------------------------------- */
/* ----------------------------------------------------------------------------- */

bool vuprs::FPGA_API__DMA__StartScatterGatherDMA_S2MM(vuprs::FPGAController *controller,
        const std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> &descriptors, bool isCyclicMode, bool enableIOCInterrupt)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }

    uint32_t descriptorSize = descriptors.size();

    if (descriptorSize == 0)
    {
        throw std::runtime_error("Descriptor is empty.");
    }
    if (isCyclicMode && descriptors[descriptorSize - 1].NXTDESC != descriptors[0].ALIGNMENT_0_CURRENT_ADDR)
    {
        throw std::runtime_error("Invalid cyclic DMA descriptor.");
    }

    uint32_t r_val, w_val;
    bool operateStatus = true;

    /* Clear buffer & reset DMA */

    vuprs::AlignedBufferDMA buffer;

    buffer.release();
    vuprs::FPGA_API__DMA__ResetDMA(controller);

    /* STEP 1: Stop DMA, clear S2MM_DMACR.RS */

    operateStatus &= controller->dev__AXI_DMA.WriteSingleRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMACR, 0, false);

    /* Wait for S2MM_DMASR.Halted = 1 */

    operateStatus &= controller->dev__AXI_DMA.WaitForRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMASR, 0, 1, 100);

    /* STEP 2: Write descriptor address to current descriptor pointer */

    operateStatus &= controller->dev__AXI_DMA.WriteSingleRegister(vuprs::AXI_DMA__Registers::S2MM_CURDESC, 0x00);  /* write with 0x00 */

    /* (STEP 2): Enable IOC interrupt */

    if (enableIOCInterrupt)
    {
        /* S2MM_DMACR.IOC_IRqEn = 1 */

        operateStatus &= controller->dev__AXI_DMA.WriteSingleRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMACR, 12, true);

        /* S2MM_DMACR.IRQThreshold = 0x01 */

        operateStatus &= controller->dev__AXI_DMA.WriteSingleRegisterBITRegion(vuprs::AXI_DMA__Registers::S2MM_DMACR, 16, 23, 0x01);

        /* S2MM_DMACR.IRQDelay = 0 */

        operateStatus &= controller->dev__AXI_DMA.WriteSingleRegisterBITRegion(vuprs::AXI_DMA__Registers::S2MM_DMACR, 24, 31, 0);
    }

    /* (STEP 2): Set Cyclic BD Enable */

    if (isCyclicMode)  /* Set bit: S2MM_DMACR.[4] */
    {
        operateStatus &= controller->dev__AXI_DMA.WriteSingleRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMACR, 4, true);
    }

    /* STEP 3: Start DMA, set S2MM_DMACR.RS = 1 */

    operateStatus &= controller->dev__AXI_DMA.WriteSingleRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMACR, 0, true);
    
    /* Wait for S2MM_DMASR.Halted = 0 */

    operateStatus &= controller->dev__AXI_DMA.WaitForRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMASR, 0, 0, 100);

    /* STEP 4: Write descriptors to SG_BRAM */

    buffer.from_vector<vuprs::AXI_DMA_ScatterGatherDescriptor>(descriptors);
    operateStatus &= controller->mem__SG_BRAM.WriteMemory(&buffer, 0x00, buffer.size());

    /* STEP 5: Write tail descriptor register to trigger. */

    if (isCyclicMode)
    {
        w_val = (uint32_t)((uint32_t)0x50 << 6);  /* Write to [31:6] */
    }
    else
    {
        w_val = (uint32_t)((uint32_t)(descriptors[descriptorSize - 1].ALIGNMENT_0_CURRENT_ADDR) << 6);  /* Write to [31:6] */
    }

    operateStatus &= controller->dev__AXI_DMA.WriteSingleRegister(vuprs::AXI_DMA__Registers::S2MM_TAILDESC, w_val);

    return operateStatus;
}

bool vuprs::FPGA_API__DMA__GetAndClearInterruptFlag(vuprs::FPGAController *controller, uint32_t* flag)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }
    if (flag == nullptr)
    {
        throw std::runtime_error("FLAG is NULL.");
    }

    uint32_t r_val;
    bool operateStatus = true;

    /* Read device to detect interrupt */

    operateStatus &= controller->dev__AXI_DMA.ReadEvent(&r_val);
    *flag = r_val;

    if (r_val == 0)  /* no interrupt */
    {
        return operateStatus;
    }

    /* Clear flags (write 1 to S2MM_DMASR.IOC_Irq) */

    operateStatus &= controller->dev__AXI_DMA.WriteSingleRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMASR, 12, true);

    return operateStatus;
}

bool vuprs::FPGA_API__DMA__ResetDMA(vuprs::FPGAController *controller)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }

    bool operateStatus = true;

    /* Reset */

    operateStatus &= controller->dev__AXI_DMA.WriteSingleRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMACR, 2, true);

    /* S2MM_DMACR.IOC_IRqEn = 0 */

    operateStatus &= controller->dev__AXI_DMA.WriteSingleRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMACR, 12, false);

    /* Wait for Halted */

    operateStatus &= controller->dev__AXI_DMA.WaitForRegisterBIT(vuprs::AXI_DMA__Registers::S2MM_DMASR, 0, 1, 100);

    return operateStatus;
}

bool vuprs::FPGA_API__DMA__GetCurrentDescriptor(vuprs::FPGAController *controller, 
        const std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> &referenceDescriptors, 
        vuprs::AXI_DMA_ScatterGatherDescriptor *currentDescriptor, 
        vuprs::AXI_DMA_ScatterGatherDescriptor *previousDescriptor,
        vuprs::AXI_DMA_ScatterGatherDescriptor *nextDescriptor)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }

    bool operateStatus = true, found = false;;
    uint32_t r_val = INVALID_SG_DESCRIPTOR_POINTER + 1;
    uint32_t nextAddr, previousAddr;

    operateStatus &= controller->dev__AXI_DMA.ReadSingleRegisterBITRegion(vuprs::AXI_DMA__Registers::S2MM_CURDESC, 6, 31, &r_val);

    /* Match */

    for (auto &descriptor: referenceDescriptors)
    {
        if (descriptor.ALIGNMENT_0_CURRENT_ADDR == r_val)
        {
            *currentDescriptor = descriptor;
            nextAddr = descriptor.NXTDESC;
            previousAddr = descriptor.ALIGNMENT_1_PREVIOUS_ADDR;
            found = true;
        }
    }
    for (auto &descriptor: referenceDescriptors)
    {
        if (descriptor.ALIGNMENT_0_CURRENT_ADDR == nextAddr)
        {
            *nextDescriptor = descriptor;
        }
        if (descriptor.ALIGNMENT_0_CURRENT_ADDR == previousAddr)
        {
            *previousDescriptor = descriptor;
        }
    }

    if (!found)
    {
        throw std::runtime_error("Cannot found current descriptor with address: " + std::to_string(r_val));
    }

    return operateStatus;
}
