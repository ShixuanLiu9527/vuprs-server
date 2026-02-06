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
    return true;
}

/* ---------------------------------------------------------------------- */
/* ---------------------------- ADC Controller -------------------------- */
/* ---------------------------------------------------------------------- */

vuprs::FPGA_Device__ADCController::FPGA_Device__ADCController()
{
    this->maxSamplingFrequencyHz = 10000.0;
    this->voltageRangeRadiusV = 10.0;
    this->workClockFrequencyHz = 50000000.0;

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
   return true;
}

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
    return true;
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
    return true;
}
