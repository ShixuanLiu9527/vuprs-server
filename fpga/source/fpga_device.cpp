#include "fpga_device.h"

/* ---------------------------------------------------------------------- */
/* -------------------------------- AXI DMA ----------------------------- */
/* ---------------------------------------------------------------------- */

vuprs::FPGA_Device__AXIDirectMemoryAccess::FPGA_Device__AXIDirectMemoryAccess()
{
    this->GenerateRegisterTable();
    this->SetRegisterOffsetDefault();
}

void vuprs::FPGA_Device__AXIDirectMemoryAccess::GenerateRegisterTable()
{
    this->registerTable = {
        {&this->offset_SG_CTL, "SG_CTL", vuprs::AXI_DMA__Registers::SG_CTL},
        {&this->offset_S2MM_DMACR, "S2MM_DMACR", vuprs::AXI_DMA__Registers::S2MM_DMACR},
        {&this->offset_S2MM_DMASR, "S2MM_DMASR", vuprs::AXI_DMA__Registers::S2MM_DMASR},
        {&this->offset_S2MM_CURDESC, "S2MM_CURDESC", vuprs::AXI_DMA__Registers::S2MM_CURDESC},
        {&this->offset_S2MM_CURDESC_MSB, "S2MM_CURDESC_MSB", vuprs::AXI_DMA__Registers::S2MM_CURDESC_MSB},
        {&this->offset_S2MM_TAILDESC, "S2MM_TAILDESC", vuprs::AXI_DMA__Registers::S2MM_TAILDESC},
        {&this->offset_S2MM_TAILDESC_MSB, "S2MM_TAILDESC_MSB", vuprs::AXI_DMA__Registers::S2MM_TAILDESC_MSB},
        {&this->offset_S2MM_DA, "S2MM_DA", vuprs::AXI_DMA__Registers::S2MM_DA},
        {&this->offset_S2MM_DA_MSB, "S2MM_DA_MSB", vuprs::AXI_DMA__Registers::S2MM_DA_MSB},
        {&this->offset_S2MM_LENGTH, "S2MM_LENGTH", vuprs::AXI_DMA__Registers::S2MM_LENGTH}
    };
}

bool vuprs::FPGA_Device__AXIDirectMemoryAccess::LoadFromJsonObj(const nlohmann::json &obj)
{
    this->LoadMainInfoFromJsonObj(obj);
    this->configdone = true;
    return true;
}

void vuprs::AXI_DMA_ScatterGatherDescriptor_ToDefault(vuprs::AXI_DMA_ScatterGatherDescriptor *descriptor)
{
    descriptor->NXTDESC = 0;
    descriptor->NXTDESC_MSB = 0;
    descriptor->BUFFER_ADDRESS = 0;
    descriptor->BUFFER_ADDRESS_MSB = 0;
    descriptor->RESERVED_0 = 0;
    descriptor->RESERVED_1 = 0;
    descriptor->CONTROL = 0;
    descriptor->STATUS = 0;

    descriptor->APP0 = 0;
    descriptor->APP1 = 0;
    descriptor->APP2 = 0;
    descriptor->APP3 = 0;
    descriptor->APP4 = 0;

    descriptor->ALIGNMENT_0_CURRENT_ADDR = 0;
    descriptor->ALIGNMENT_1 = 0;
    descriptor->ALIGNMENT_2 = 0;
}

void vuprs::CreateDMAScatterGatherDescriptorChain(std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> *descriptorList, 
        uint32_t bufferSize, uint32_t bufferCount, uint32_t ddrBaseAddr, bool isCyclicMode)
{
    if (bufferSize == 0 || bufferCount == 0)
    {
        throw std::runtime_error("Buffer size or buffer count should not be 0.");
    }
    if (bufferSize % vuprs::DMA_BUFFER_ALIGNMENT_1_WORD != 0)
    {
        throw std::runtime_error("Buffer size must aligned to 1 word");
    }
    if (bufferSize > vuprs::DMA_MAX_BUFFER_LENGTH)
    {
        throw std::runtime_error("Buffer size must be smaller than " + std::to_string(vuprs::DMA_MAX_BUFFER_LENGTH) + " bytes.");
    }

    descriptorList->resize(bufferCount);

    for (uint32_t i = 0; i < bufferCount; i++)
    {
        vuprs::AXI_DMA_ScatterGatherDescriptor_ToDefault(&(*descriptorList)[i]);

        /* Next descriptor address (in bytes) = (i + 1) * 64U. */

        (*descriptorList)[i].NXTDESC = (i + 1) * sizeof(vuprs::AXI_DMA_ScatterGatherDescriptor);
        (*descriptorList)[i].ALIGNMENT_0_CURRENT_ADDR = i * sizeof(vuprs::AXI_DMA_ScatterGatherDescriptor);

        /* Current buffer address (in bytes) = i * buffer size */

        (*descriptorList)[i].BUFFER_ADDRESS = i * bufferSize + ddrBaseAddr;

        /* Current buffer length CONTROL[25:0] = buffer size */

        (*descriptorList)[i].CONTROL |= (bufferSize & vuprs::DMA_BUFFER_LENGTH_MASK);
    }
    if (isCyclicMode)
    {
        (*descriptorList)[bufferCount - 1].NXTDESC = 0;  /* point to begining */
    }
}

/* ---------------------------------------------------------------------- */
/* ---------------------------- ADC Controller -------------------------- */
/* ---------------------------------------------------------------------- */

vuprs::FPGA_Device__ADCController::FPGA_Device__ADCController()
{
    this->maxSamplingFrequencyHz = 10000.0;
    this->voltageRangeRadiusV = 10.0;
    this->workClockFrequencyHz = 50000000.0;

    this->currentSamplingFrequency = 0.0;
    this->currentSCI = 0xFFFFFFFF;

    this->GenerateRegisterTable();
    this->SetRegisterOffsetDefault();
}

void vuprs::FPGA_Device__ADCController::GenerateRegisterTable()
{
    this->registerTable = {
        {&this->offset_ADC_SCI, "ADC_SCI", vuprs::ADC_Controller__Registers::ADC_SCI},
        {&this->offset_ADC_SP, "ADC_SP", vuprs::ADC_Controller__Registers::ADC_SP},
        {&this->offset_ADC_SF, "ADC_SF", vuprs::ADC_Controller__Registers::ADC_SF},
        {&this->offset_ADC_STR, "ADC_STR", vuprs::ADC_Controller__Registers::ADC_STR},
        {&this->offset_ADC_NGF, "ADC_NGF", vuprs::ADC_Controller__Registers::ADC_NGF},
        {&this->offset_ADC_ERR, "ADC_ERR", vuprs::ADC_Controller__Registers::ADC_ERR},
        {&this->offset_ADC_RST, "ADC_RST", vuprs::ADC_Controller__Registers::ADC_RST},
        {&this->offset_ADC_CS, "ADC_CS", vuprs::ADC_Controller__Registers::ADC_CS}
    };
}

bool vuprs::FPGA_Device__ADCController::LoadFromJsonObj(const nlohmann::json &obj)
{
   this->LoadMainInfoFromJsonObj(obj);
   vuprs::__JsonStringParseFLOAT<double>(&this->maxSamplingFrequencyHz, obj, "max-sampling-frequency-hz", true);
   vuprs::__JsonStringParseFLOAT<double>(&this->voltageRangeRadiusV, obj, "voltage-range-radius-v", true);
   vuprs::__JsonStringParseFLOAT<double>(&this->workClockFrequencyHz, obj, "work-clock-frequency-hz", true);
   this->configdone = true;
   return true;
}

uint32_t vuprs::FPGA_Device__ADCController::GetSCIValueForSamplingFrequency(double fs) const
{
    if (!this->configdone)
    {
        throw std::runtime_error("Config not complete.");
    }
    if (fs > this->maxSamplingFrequencyHz || fs <= 1e-2)
    {
        return 0xFFFFFFFF;
    }

    uint32_t SCI = std::round(this->workClockFrequencyHz / (2.0 * fs));
    uint32_t SCI_u = SCI + 1;
    uint32_t SCI_l = SCI - 1;

    double f_SCI = this->SCI2FS(SCI);
    double f_SCI_u = this->SCI2FS(SCI_u);
    double f_SCI_l = this->SCI2FS(SCI_l);

    if (abs(f_SCI - fs) <= abs(f_SCI_u - fs) && 
        abs(f_SCI - fs) <= abs(f_SCI_l - fs))
    {
        return SCI;
    }
    else if (abs(f_SCI_u - fs) <= abs(f_SCI - fs) && 
             abs(f_SCI_u - fs) <= abs(f_SCI_l - fs))
    {
        return SCI_u;
    }
    else if (abs(f_SCI_l - fs) <= abs(f_SCI - fs) && 
             abs(f_SCI_l - fs) <= abs(f_SCI_u - fs))
    {
        return SCI_l;
    }
    
    return SCI;
}

double vuprs::FPGA_Device__ADCController::SCI2FS(uint32_t SCI) const
{
    return this->workClockFrequencyHz / (2.0 * static_cast<double>(SCI));
}

void vuprs::FPGA_Device__ADCController::SetSCI(uint32_t SCI) 
{
    this->currentSCI = SCI;
    this->currentSamplingFrequency = this->SCI2FS(SCI);
}

double vuprs::FPGA_Device__ADCController::MaxSamplingFrequency() const {return this->maxSamplingFrequencyHz;}
double vuprs::FPGA_Device__ADCController::VoltageRangeRadius() const {return this->voltageRangeRadiusV;}
double vuprs::FPGA_Device__ADCController::WorkFrequency() const {return this->workClockFrequencyHz;}
double vuprs::FPGA_Device__ADCController::CurrentSamplingFrequency() const {return this->currentSamplingFrequency;}

/* ---------------------------------------------------------------------- */
/* --------------------------- Circular Buffer -------------------------- */
/* ---------------------------------------------------------------------- */

vuprs::FPGA_Device__CircularBuffer::FPGA_Device__CircularBuffer()
{
    this->GenerateRegisterTable();
    this->SetRegisterOffsetDefault();
}

void vuprs::FPGA_Device__CircularBuffer::GenerateRegisterTable()
{
    this->registerTable = {
        {&this->offset_CBUF_FREEZE, "CBUF_FREEZE", vuprs::Circular_Buffer__Registers::CBUF_FREEZE},
        {&this->offset_CBUF_RST, "CBUF_RST", vuprs::Circular_Buffer__Registers::CBUF_RST},
        {&this->offset_CBUF_RS, "CBUF_RS", vuprs::Circular_Buffer__Registers::CBUF_RS},
        {&this->offset_CBUF_CBP, "CBUF_CBP", vuprs::Circular_Buffer__Registers::CBUF_CBP}
    };
}

bool vuprs::FPGA_Device__CircularBuffer::LoadFromJsonObj(const nlohmann::json &obj)
{
    this->LoadMainInfoFromJsonObj(obj);
    vuprs::__JsonStringParseINT<uint32_t>(&this->signalPoints, obj, "signal-points", true);
    this->configdone = true;
    return true;
}

uint32_t vuprs::FPGA_Device__CircularBuffer::SignalPoints() const
{
    return this->signalPoints;
}

/* ---------------------------------------------------------------------- */
/* --------------------------- FIR Filter Bank -------------------------- */
/* ---------------------------------------------------------------------- */

vuprs::FPGA_Device__FIRFilterBank::FPGA_Device__FIRFilterBank()
{
    this->GenerateRegisterTable();
    this->SetRegisterOffsetDefault();
}

void vuprs::FPGA_Device__FIRFilterBank::GenerateRegisterTable()
{
    this->registerTable = {
        {&this->offset_FIR_RST, "FIR_RST", vuprs::FIR_Filter_Bank__Registers::FIR_RST},
        {&this->offset_FIR_U_FIR_LEN, "FIR_U_FIR_LEN", vuprs::FIR_Filter_Bank__Registers::FIR_U_FIR_LEN},
        {&this->offset_FIR_U_FIR_COEF, "FIR_U_FIR_COEF", vuprs::FIR_Filter_Bank__Registers::FIR_U_FIR_COEF},
        {&this->offset_FIR_LEN, "FIR_LEN", vuprs::FIR_Filter_Bank__Registers::FIR_LEN},
        {&this->offset_FIR_COEF_SCALE, "FIR_COEF_SCALE", vuprs::FIR_Filter_Bank__Registers::FIR_COEF_SCALE},
        {&this->offset_FIR_RSC, "FIR_RSC", vuprs::FIR_Filter_Bank__Registers::FIR_RSC},
        {&this->offset_FIR_RS, "FIR_RS", vuprs::FIR_Filter_Bank__Registers::FIR_RS},
        {&this->offset_FIR_MAX_LEN, "FIR_MAX_LEN", vuprs::FIR_Filter_Bank__Registers::FIR_MAX_LEN}
    };
}

bool vuprs::FPGA_Device__FIRFilterBank::LoadFromJsonObj(const nlohmann::json &obj)
{
    this->LoadMainInfoFromJsonObj(obj);
    this->configdone = true;
    return true;
}

/* ---------------------------------------------------------------------- */
/* --------------------------- Pre-delay Unit --------------------------- */
/* ---------------------------------------------------------------------- */

vuprs::FPGA_Device__PreDelayUnit::FPGA_Device__PreDelayUnit()
{
    this->GenerateRegisterTable();
    this->SetRegisterOffsetDefault();
}

void vuprs::FPGA_Device__PreDelayUnit::GenerateRegisterTable()
{
    this->registerTable = {
        {&this->offset_PREDLY_CH1_CH2, "PREDLY_CH1_CH2", vuprs::PreDelay_Unit__Registers::PREDLY_CH1_CH2},
        {&this->offset_PREDLY_CH3_CH4, "PREDLY_CH3_CH4", vuprs::PreDelay_Unit__Registers::PREDLY_CH3_CH4},
        {&this->offset_PREDLY_CH5_CH6, "PREDLY_CH5_CH6", vuprs::PreDelay_Unit__Registers::PREDLY_CH5_CH6},
        {&this->offset_PREDLY_CH7_CH8, "PREDLY_CH7_CH8", vuprs::PreDelay_Unit__Registers::PREDLY_CH7_CH8},
        {&this->offset_PREDLY_CH9_CH10, "PREDLY_CH9_CH10", vuprs::PreDelay_Unit__Registers::PREDLY_CH9_CH10},
        {&this->offset_PREDLY_CH11_CH12, "PREDLY_CH11_CH12", vuprs::PreDelay_Unit__Registers::PREDLY_CH11_CH12},
        {&this->offset_PREDLY_CH13_CH14, "PREDLY_CH13_CH14", vuprs::PreDelay_Unit__Registers::PREDLY_CH13_CH14},
        {&this->offset_PREDLY_CH15_CH16, "PREDLY_CH15_CH16", vuprs::PreDelay_Unit__Registers::PREDLY_CH15_CH16},
        {&this->offset_PREDLY_FREEZE, "PREDLY_FREEZE", vuprs::PreDelay_Unit__Registers::PREDLY_FREEZE},
        {&this->offset_PREDLY_RST, "PREDLY_RST", vuprs::PreDelay_Unit__Registers::PREDLY_RST},
        {&this->offset_PREDLY_RS, "PREDLY_RS", vuprs::PreDelay_Unit__Registers::PREDLY_RS},
        {&this->offset_PREDLY_MAX_DLY, "PREDLY_MAX_DLY", vuprs::PreDelay_Unit__Registers::PREDLY_MAX_DLY}
    };
}

bool vuprs::FPGA_Device__PreDelayUnit::LoadFromJsonObj(const nlohmann::json &obj)
{
    this->LoadMainInfoFromJsonObj(obj);
    this->configdone = true;
    return true;
}
