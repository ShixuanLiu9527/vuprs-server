#include "fpga_api.h"

bool vuprs::FPGA_API__ADC__StartADC(vuprs::FPGAController *controller, double fs)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }

    uint32_t r_val, w_val;
    int timeout = 0;
    bool operateStatus = true;

    /* STEP 1: Reset ADC */

    operateStatus &= controller->dev__ADC_Controller.WriteSingleRegister(vuprs::ADC_Controller__Registers::ADC_RST, 0);

    /* STEP 2: Wait for stop */

    do
    {
        operateStatus &= controller->dev__ADC_Controller.ReadSingleRegister(vuprs::ADC_Controller__Registers::ADC_STR, &r_val);
        if (FPGA_REG_BIT(r_val, 0)) break;
        if (timeout > 1000) return false;
        timeout++;
        usleep(1000);
    } 
    while (!FPGA_REG_BIT(r_val, 0));

    /* STEP 3: Set sampling frequency fs */

    w_val = controller->dev__ADC_Controller.GetSCIValueForSamplingFrequency(fs);
    operateStatus &= controller->dev__ADC_Controller.WriteSingleRegister(vuprs::ADC_Controller__Registers::ADC_SCI, w_val);
    
    controller->dev__ADC_Controller.SetSCI(w_val);
    
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

    uint32_t r_val;
    int timeout = 0;
    bool operateStatus = true;

    /* STEP 1: Reset ADC */

    operateStatus &= controller->dev__ADC_Controller.WriteSingleRegister(vuprs::ADC_Controller__Registers::ADC_RST, 0);

    /* STEP 2: Wait for stop */

    do
    {
        operateStatus &= controller->dev__ADC_Controller.ReadSingleRegister(vuprs::ADC_Controller__Registers::ADC_STR, &r_val);
        if (FPGA_REG_BIT(r_val, 0)) break;
        if (timeout > 1000) return false;
        timeout++;
        usleep(1000);
    } 
    while (!FPGA_REG_BIT(r_val, 0));

    return operateStatus;
}

bool vuprs::FPGA_API__CBUF__ReadCircularBuffer(vuprs::FPGAController *controller, 
    vuprs::SignalData *signal)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }

    uint32_t r_val, w_val;
    int timeout = 0;
    bool operateStatus = true;

    /* Clear buffer */

    controller->buffer.release();

    /* STEP 1: Wait refreshed */

    do
    {
        operateStatus &= controller->dev__Circular_Buffer.ReadSingleRegister(vuprs::Circular_Buffer__Registers::CBUF_RS, &r_val);
        if (FPGA_REG_BIT(r_val, 1)) break;
        if (timeout > 1000) return false;
        timeout++;
        usleep(1000);
    } 
    while (!FPGA_REG_BIT(r_val, 1));

    /* STEP 2: Freeze */

    operateStatus &= controller->dev__Circular_Buffer.WriteSingleRegister(vuprs::Circular_Buffer__Registers::CBUF_FREEZE, 0x00000001);

    /* STEP 3: Wait for freezed */

    timeout = 0;

    do
    {
        operateStatus &= controller->dev__Circular_Buffer.ReadSingleRegister(vuprs::Circular_Buffer__Registers::CBUF_RS, &r_val);
        if (FPGA_REG_BIT(r_val, 0)) break;
        if (timeout > 1000) return false;
        timeout++;
        usleep(1000);
    } 
    while (!FPGA_REG_BIT(r_val, 0));

    /* STEP 3: Read circular buffer */
    uint32_t signalPoints = controller->dev__Circular_Buffer.SignalPoints();
    operateStatus &= controller->mem__Circular_Buffer_BRAM.ReadMemory(&controller->buffer, 0, signalPoints * ADC_FRAME_WORD_SIZE * sizeof(uint32_t));

    /* STEP 4: Read current BRAM pointer */

    operateStatus &= controller->dev__Circular_Buffer.ReadSingleRegister(vuprs::Circular_Buffer__Registers::CBUF_CBP, &r_val);

    /* STEP 5: Reset */

    operateStatus &= controller->dev__Circular_Buffer.WriteSingleRegister(vuprs::Circular_Buffer__Registers::CBUF_RST, 0);

    /* Data convert */

    double voltageScale = controller->dev__ADC_Controller.VoltageRangeRadius();
    double fs = controller->dev__ADC_Controller.CurrentSamplingFrequency();

    operateStatus &= vuprs::FPGACircularBuffer2Frames(&controller->buffer, signal, fs, voltageScale, r_val);

    return operateStatus;
}

bool vuprs::FPGA_API__PDLY__SetPredelay(vuprs::FPGAController *controller, 
    const std::vector<uint16_t> &channelPredelay, const std::vector<std::string> &channelName)
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
    int timeout = 0;
    
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
            predelayOrdered[i] = channelPredelay[pos];
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

    /* Clear buffer */

    controller->buffer.release();

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

    controller->buffer.from_vector<uint32_t>(coefficientsToWrite);

    /* Write coefficients to BRAM */

    operateStatus &= controller->mem__FIR_BRAM.WriteMemory(&controller->buffer, 0, totalCoefficientsCount * sizeof(uint32_t));

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

    controller->buffer.release();

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

    controller->buffer.from_vector<uint32_t>(coefficientsToWrite);

    /* Write coefficients to BRAM */

    operateStatus &= controller->mem__FIR_BRAM.WriteMemory(&controller->buffer, 0, totalCoefficientsCount * sizeof(uint32_t));

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

bool vuprs::FPGA_API__FIR__ReadDDR(vuprs::FPGAController *controller, 
    vuprs::AlignedBufferDMA *buffer, uint32_t ddrOffset, uint32_t transferSize)
{
    if (!controller->ConfigDown())
    {
        throw std::runtime_error("FPGA Controller not configured in advance.");
    }

    return controller->mem__DDR.ReadMemory(buffer, ddrOffset, transferSize);
}
