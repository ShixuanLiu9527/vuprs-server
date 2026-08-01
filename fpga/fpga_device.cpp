#include "fpga/fpga_device.h"
#include "logger/check.h"

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
    std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
    this->register_table = {
        {&this->offset_SG_CTL, "SG_CTL", vuprs::AXI_DMA__Registers::SG_CTL},
        {&this->offset_S2MM_DMACR, "S2MM_DMACR", vuprs::AXI_DMA__Registers::S2MM_DMACR},
        {&this->offset_S2MM_DMASR, "S2MM_DMASR", vuprs::AXI_DMA__Registers::S2MM_DMASR},
        {&this->offset_S2MM_CURDESC, "S2MM_CURDESC", vuprs::AXI_DMA__Registers::S2MM_CURDESC},
        {&this->offset_S2MM_CURDESC_MSB, "S2MM_CURDESC_MSB", vuprs::AXI_DMA__Registers::S2MM_CURDESC_MSB},
        {&this->offset_S2MM_TAILDESC, "S2MM_TAILDESC", vuprs::AXI_DMA__Registers::S2MM_TAILDESC},
        {&this->offset_S2MM_TAILDESC_MSB, "S2MM_TAILDESC_MSB", vuprs::AXI_DMA__Registers::S2MM_TAILDESC_MSB},
        {&this->offset_S2MM_DA, "S2MM_DA", vuprs::AXI_DMA__Registers::S2MM_DA},
        {&this->offset_S2MM_DA_MSB, "S2MM_DA_MSB", vuprs::AXI_DMA__Registers::S2MM_DA_MSB},
        {&this->offset_S2MM_LENGTH, "S2MM_LENGTH", vuprs::AXI_DMA__Registers::S2MM_LENGTH}};
}

bool vuprs::FPGA_Device__AXIDirectMemoryAccess::LoadFromJsonObj(const nlohmann::json &obj)
{
    this->LoadMainInfoFromJsonObj(obj);
    {
        std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
        vuprs::__JsonStringParseINT<uint32_t>(&this->s2mm_transfer_register_length, obj, "s2mm-length-register-width", true);
    }
    this->config_done = true;
    return true;
}

void vuprs::AXI_DMA_ScatterGatherDescriptor_ToDefault(vuprs::AXI_DMA_ScatterGatherDescriptor *descriptor)
{
    memset(descriptor, 0, sizeof(vuprs::AXI_DMA_ScatterGatherDescriptor));
}

void vuprs::CreateDMAScatterGatherDescriptorChain(std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> *descriptor_list,
                                                  const vuprs::AXI_DMA_SGDescriptor_Config &config)
{
    PARAM_CHECK(!(config.buffer_size == 0 || config.buffer_count == 0), "fpga", " in [vuprs::CreateDMAScatterGatherDescriptorChain] Buffer size or buffer count should not be 0.");
    PARAM_CHECK(config.buffer_size % vuprs::DMA_BUFFER_ALIGNMENT_1_WORD == 0, "fpga", " in [vuprs::CreateDMAScatterGatherDescriptorChain] Buffer size must aligned to 1 word");
    PARAM_CHECK(config.buffer_size <= vuprs::DMA_MAX_BUFFER_LENGTH, "fpga", " in [vuprs::CreateDMAScatterGatherDescriptorChain] Buffer size must be smaller than " + std::to_string(vuprs::DMA_MAX_BUFFER_LENGTH) + " bytes.");

    descriptor_list->resize(config.buffer_count);
    /* Operate as normal link list */
    for (uint32_t i = 0; i < config.buffer_count; i++)
    {
        vuprs::AXI_DMA_ScatterGatherDescriptor_ToDefault(&(*descriptor_list)[i]);
        /* Next descriptor address (in bytes) = (i + 1) * 64U. */
        (*descriptor_list)[i].NXTDESC = (i + 1) * sizeof(vuprs::AXI_DMA_ScatterGatherDescriptor) + config.sg_bram_fpga_base_addr;
        /* Current descriptor address */
        (*descriptor_list)[i].ALIGNMENT_0_CURRENT_ADDR = i * sizeof(vuprs::AXI_DMA_ScatterGatherDescriptor) + config.sg_bram_fpga_base_addr;
        /* Previous descriptor address */
        if (i == 0)
        {
            (*descriptor_list)[i].ALIGNMENT_1_PREVIOUS_ADDR = INVALID_SG_DESCRIPTOR_POINTER;
        }
        else
        {
            (*descriptor_list)[i].ALIGNMENT_1_PREVIOUS_ADDR = (i - 1) * sizeof(vuprs::AXI_DMA_ScatterGatherDescriptor) + config.sg_bram_fpga_base_addr;
        }
        /* Current buffer address (in bytes) = i * buffer size */
        (*descriptor_list)[i].BUFFER_ADDRESS = i * config.buffer_size + config.ddr_fpga_base_addr;
        (*descriptor_list)[i].ALIGNMENT_2_BUFFER_SIZE = config.buffer_size;
        /* Current buffer length CONTROL[25:0] = buffer size */
        (*descriptor_list)[i].CONTROL |= (config.buffer_size & vuprs::DMA_BUFFER_LENGTH_MASK);
    }
    /* Special operation for Cyclic DMA Mode */
    if (config.is_cyclic_dma_mode)
    {
        (*descriptor_list)[0].ALIGNMENT_1_PREVIOUS_ADDR = (*descriptor_list)[config.buffer_count - 1].ALIGNMENT_0_CURRENT_ADDR;
        (*descriptor_list)[config.buffer_count - 1].NXTDESC = (*descriptor_list)[0].ALIGNMENT_0_CURRENT_ADDR; /* point to begining */
    }
}

bool vuprs::MatchDescriptor(const std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> &descriptor_list, uint32_t currentDescriptorAddr,
                            vuprs::AXI_DMA_ScatterGatherDescriptor *cur_desc,
                            vuprs::AXI_DMA_ScatterGatherDescriptor *next_desc,
                            vuprs::AXI_DMA_ScatterGatherDescriptor *prev_desc)
{
    uint32_t next_addr, previous_addr;
    bool find_cur = false, find_next = false, find_prev = false;
    PARAM_CHECK(cur_desc != nullptr && next_desc != nullptr && prev_desc != nullptr, "fpga", " in [vuprs::MatchDescriptor] target descriptor is NULL.");
    for (auto &desc : descriptor_list)
    {
        if (desc.ALIGNMENT_0_CURRENT_ADDR == currentDescriptorAddr)
        {
            next_addr = desc.NXTDESC;
            previous_addr = desc.ALIGNMENT_1_PREVIOUS_ADDR;
            *cur_desc = desc;
            find_cur = true;
            break;
        }
    }
    for (auto &desc : descriptor_list)
    {
        if (desc.ALIGNMENT_0_CURRENT_ADDR == next_addr)
        {
            *next_desc = desc;
            find_next = true;
            if (find_prev)
                break;
        }
        if (desc.ALIGNMENT_0_CURRENT_ADDR == previous_addr)
        {
            *prev_desc = desc;
            find_prev = true;
            if (find_next)
                break;
        }
    }

    return find_cur && find_next && find_prev;
}

/* ---------------------------------------------------------------------- */
/* ---------------------------- ADC Controller -------------------------- */
/* ---------------------------------------------------------------------- */

vuprs::FPGA_Device__ADCController::FPGA_Device__ADCController()
{
    {
        std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
        this->max_fs = 10000.0;
        this->voltage_range_radius = 10.0;
        this->work_clock_frequency = 50000000.0;
    }

    this->GenerateRegisterTable();
    this->SetRegisterOffsetDefault();
}

void vuprs::FPGA_Device__ADCController::GenerateRegisterTable()
{
    std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
    this->register_table = {
        {&this->offset_ADC_SCI, "ADC_SCI", vuprs::ADC_Controller__Registers::ADC_SCI},
        {&this->offset_ADC_SP, "ADC_SP", vuprs::ADC_Controller__Registers::ADC_SP},
        {&this->offset_ADC_SF, "ADC_SF", vuprs::ADC_Controller__Registers::ADC_SF},
        {&this->offset_ADC_STR, "ADC_STR", vuprs::ADC_Controller__Registers::ADC_STR},
        {&this->offset_ADC_NGF, "ADC_NGF", vuprs::ADC_Controller__Registers::ADC_NGF},
        {&this->offset_ADC_ERR, "ADC_ERR", vuprs::ADC_Controller__Registers::ADC_ERR},
        {&this->offset_ADC_RST, "ADC_RST", vuprs::ADC_Controller__Registers::ADC_RST},
        {&this->offset_ADC_CS, "ADC_CS", vuprs::ADC_Controller__Registers::ADC_CS}};
}

bool vuprs::FPGA_Device__ADCController::LoadFromJsonObj(const nlohmann::json &obj)
{
    this->LoadMainInfoFromJsonObj(obj);
    {
        std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
        vuprs::__JsonStringParseFLOAT<double>(&this->max_fs, obj, "max-sampling-frequency-hz", true);
        vuprs::__JsonStringParseFLOAT<double>(&this->voltage_range_radius, obj, "voltage-range-radius-v", true);
        vuprs::__JsonStringParseFLOAT<double>(&this->work_clock_frequency, obj, "work-clock-frequency-hz", true);
    }
    this->config_done = true;
    return true;
}

uint32_t vuprs::FPGA_Device__ADCController::GetSCIValueForSamplingFrequency(double fs) const
{
    PARAM_CHECK(this->config_done, "fpga", " in [vuprs::FPGA_Device__ADCController] Config not complete.");
    if (fs > this->max_fs || fs <= 1e-2)
    {
        return 0xFFFFFFFF;
    }
    uint32_t SCI;
    {
        std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
        SCI = std::round(this->work_clock_frequency / (2.0 * fs));
    }
    uint32_t SCI_u = SCI + 1;
    uint32_t SCI_l = SCI - 1;
    double f_SCI = this->SCI2FS(SCI);
    double f_SCI_u = this->SCI2FS(SCI_u);
    double f_SCI_l = this->SCI2FS(SCI_l);
    if (std::abs(f_SCI - fs) <= std::abs(f_SCI_u - fs) &&
        std::abs(f_SCI - fs) <= std::abs(f_SCI_l - fs))
    {
        return SCI;
    }
    else if (std::abs(f_SCI_u - fs) <= std::abs(f_SCI - fs) &&
             std::abs(f_SCI_u - fs) <= std::abs(f_SCI_l - fs))
    {
        return SCI_u;
    }
    else if (std::abs(f_SCI_l - fs) <= std::abs(f_SCI - fs) &&
             std::abs(f_SCI_l - fs) <= std::abs(f_SCI_u - fs))
    {
        return SCI_l;
    }
    return SCI;
}

double vuprs::FPGA_Device__ADCController::SCI2FS(uint32_t SCI) const
{
    std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
    return this->work_clock_frequency / (2.0 * static_cast<double>(SCI));
}

double vuprs::FPGA_Device__ADCController::MaxSamplingFrequency() const
{
    std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
    return this->max_fs;
}

double vuprs::FPGA_Device__ADCController::VoltageRangeRadius() const
{
    std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
    return this->voltage_range_radius;
}

double vuprs::FPGA_Device__ADCController::WorkFrequency() const
{
    std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
    return this->work_clock_frequency;
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
    std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
    this->register_table = {
        {&this->offset_CBUF_FREEZE, "CBUF_FREEZE", vuprs::Circular_Buffer__Registers::CBUF_FREEZE},
        {&this->offset_CBUF_RST, "CBUF_RST", vuprs::Circular_Buffer__Registers::CBUF_RST},
        {&this->offset_CBUF_RS, "CBUF_RS", vuprs::Circular_Buffer__Registers::CBUF_RS},
        {&this->offset_CBUF_CBP, "CBUF_CBP", vuprs::Circular_Buffer__Registers::CBUF_CBP}};
}

bool vuprs::FPGA_Device__CircularBuffer::LoadFromJsonObj(const nlohmann::json &obj)
{
    this->LoadMainInfoFromJsonObj(obj);
    {
        std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
        vuprs::__JsonStringParseINT<uint32_t>(&this->signal_points, obj, "signal-points", true);
    }
    this->config_done = true;
    return true;
}

uint32_t vuprs::FPGA_Device__CircularBuffer::SignalPoints() const
{
    std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
    return this->signal_points;
}

bool vuprs::FPGA_Device__CircularBuffer::Refreshed()
{
    uint32_t r_val;
    bool operation - status;
    operation - status = this->ReadSingleRegisterBIT(vuprs::Circular_Buffer__Registers::CBUF_RS, 1, &r_val);
    RUNTIME_CHECK(operation - status, "fpga", " in [FPGA_Device__CircularBuffer::Refreshed] Cannot read circular buffer.");
    return r_val == 1;
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
    std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
    this->register_table = {
        {&this->offset_FIR_RST, "FIR_RST", vuprs::FIR_Filter_Bank__Registers::FIR_RST},
        {&this->offset_FIR_U_FIR_LEN, "FIR_U_FIR_LEN", vuprs::FIR_Filter_Bank__Registers::FIR_U_FIR_LEN},
        {&this->offset_FIR_U_FIR_COEF, "FIR_U_FIR_COEF", vuprs::FIR_Filter_Bank__Registers::FIR_U_FIR_COEF},
        {&this->offset_FIR_LEN, "FIR_LEN", vuprs::FIR_Filter_Bank__Registers::FIR_LEN},
        {&this->offset_FIR_COEF_SCALE, "FIR_COEF_SCALE", vuprs::FIR_Filter_Bank__Registers::FIR_COEF_SCALE},
        {&this->offset_FIR_RSC, "FIR_RSC", vuprs::FIR_Filter_Bank__Registers::FIR_RSC},
        {&this->offset_FIR_RS, "FIR_RS", vuprs::FIR_Filter_Bank__Registers::FIR_RS},
        {&this->offset_FIR_MAX_LEN, "FIR_MAX_LEN", vuprs::FIR_Filter_Bank__Registers::FIR_MAX_LEN}};
}

bool vuprs::FPGA_Device__FIRFilterBank::LoadFromJsonObj(const nlohmann::json &obj)
{
    this->LoadMainInfoFromJsonObj(obj);
    this->config_done = true;
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
    std::unique_lock<std::mutex> lock(this->mut); /* LOCK */
    this->register_table = {
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
        {&this->offset_PREDLY_MAX_DLY, "PREDLY_MAX_DLY", vuprs::PreDelay_Unit__Registers::PREDLY_MAX_DLY}};
}

bool vuprs::FPGA_Device__PreDelayUnit::LoadFromJsonObj(const nlohmann::json &obj)
{
    this->LoadMainInfoFromJsonObj(obj);
    this->config_done = true;
    return true;
}
