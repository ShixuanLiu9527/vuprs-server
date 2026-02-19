#include "arm_fpga_bf_collab.h"

void vuprs::Set_ARM_FPGA_BF_Config_ToDefault(vuprs::ARM_FPGA_BF_Config *config)
{
    config->fs = 1000.0;  /* sampling frequency (unit: Hz) */
    config->bf_target__alt = 90.0;  /* altitude (unit: degree) beam former pointing target */
    config->bf_target__az = 0.0;  /* azimuth (unit: degree) beam former pointing target */
    config->bf_waveVelocity = 346.0;
    config->bf_freq__lower = 100.0;  /* lower boundary of beam former work frequency (unit: Hz) */
    config->bf_freq__upper = 5000.0;  /* upper boundary of beam former work frequency (unit: Hz) */
    config->bf_cov_snapshotsWindowSize = 100;  /* Snapshots window size (to fit covariance matrix) */
    config->bf_cov_freqAverageIndex = 0.8;  /* frequency average index (to fit covariance matrix) */
    config->dma__bufferSize = 32768;  /* AXI DMA descriptor buffer size */
    config->dma__bufferCount = 10;  /* AXI DMA descriptor buffer count */
}

bool vuprs::_Check_ARM_FPGA_BF_Config_Valid(const vuprs::ARM_FPGA_BF_Config &config)
{

}

vuprs::ARM_FPGA_CollaborationBeamfomer::ARM_FPGA_CollaborationBeamfomer()
{
    this->configdone = false;
    this->beamformerStarted = false;
    this->hardwareSamplingFrequency = 0.0;
    this->firstCoefficientsIssued = false;
}

vuprs::ARM_FPGA_CollaborationBeamfomer::~ARM_FPGA_CollaborationBeamfomer()
{

}

bool vuprs::ARM_FPGA_CollaborationBeamfomer::ConfigDone() const
{
    return this->configdone;
}

bool vuprs::ARM_FPGA_CollaborationBeamfomer::BeamFormerHaveValidOutput() const
{
    return this->firstCoefficientsIssued;
}

bool vuprs::ARM_FPGA_CollaborationBeamfomer::InitCollaborationBeamfomer(const std::string &fpgaConfigJson, const std::string &bfArrayConfigJson, const std::string &firConfigJon)
{
    bool operateStatus = true;
    try
    {
        operateStatus &= this->controller.ConfigFPGAFromJson(fpgaConfigJson);
        operateStatus &= this->bf_dcrcb.ConfigArrayFromJson(bfArrayConfigJson);
        operateStatus &= this->fir.ConfigFIRFromJsonFile(firConfigJon);
    }
    catch(const std::exception& e)
    {
        throw std::runtime_error("Error occurred in initialization.");
    }

    this->configdone = operateStatus;
    return operateStatus;
}

void vuprs::ARM_FPGA_CollaborationBeamfomer::GetDMADescriptor(std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> *output) const
{
    *output = this->dmaDescriptors;
}

bool vuprs::ARM_FPGA_CollaborationBeamfomer::ResetHardwareBeamformer()
{
    bool retval = true;

    /* FPGA reset */

    retval &= vuprs::FPGA_API__ADC__ResetADC(&this->controller);  /* Reset ADC controller */
    retval &= vuprs::FPGA_API__CBUF__ResetCircularBuffer(&this->controller);  /* Reset Circular Buffer */
    retval &= vuprs::FPGA_API__FIR__ResetFIR(&this->controller);  /* Reset FIR Filter Bank */
    retval &= vuprs::FPGA_API__DMA__ResetDMA(&this->controller);  /* Reset AXI DMA */

    /* Algorithm reset */

    this->bf_dcrcb.ResetAll();

    this->beamformerStarted = false;

    return retval;
}

bool vuprs::ARM_FPGA_CollaborationBeamfomer::StartBeamformer(const ARM_FPGA_BF_Config &config)
{
    if (!this->ConfigDone())
    {
        throw std::runtime_error("Config not complete.");
    }

    bool retval = true;

    /* Generate descriptors */

    this->sg_descriptorConfig.bufferCount = config.dma__bufferCount;
    this->sg_descriptorConfig.bufferSize = config.dma__bufferSize;
    this->sg_descriptorConfig.ddr_FPGABaseAddr = this->controller.mem__DDR.FPGAAddress();
    this->sg_descriptorConfig.sgBRAM_FPGABaseAddr = this->controller.mem__SG_BRAM.FPGAAddress();

    this->sg_descriptorConfig.isCyclicDMAMode = true;

    vuprs::CreateDMAScatterGatherDescriptorChain(&this->dmaDescriptors, this->sg_descriptorConfig);

    /* Set algorithm parameters */

    this->fir.SetFrequencyRange(config.bf_freq__lower, config.bf_freq__upper);

    this->bf_dcrcb.ResetCovarianceMatrices();
    this->bf_dcrcb.SetCovarianceMatrixFittingParam(config.bf_cov_snapshotsWindowSize, config.bf_cov_freqAverageIndex);
    this->bf_dcrcb.SetTargetDirection(config.bf_target__alt, config.bf_target__az, config.bf_waveVelocity);

    /* Get predelay */

    uint32_t SCI = this->controller.dev__ADC_Controller.GetSCIValueForSamplingFrequency(config.fs);
    this->hardwareSamplingFrequency = this->controller.dev__ADC_Controller.SCI2FS(SCI);

    this->bf_dcrcb.UpdateAndGetElementPredelay(this->fir.FIRLength(), this->hardwareSamplingFrequency, 
        &this->predelayCount, &this->predelayTime, &this->channelName);

    /* System reset FPGA */

    retval &= this->ResetHardwareBeamformer();

    /* FPGA: STEP1 - Config DMA */

    retval &= vuprs::FPGA_API__DMA__StartScatterGatherDMA_S2MM(&this->controller, this->dmaDescriptors, true);

    /* FPGA: STEP 2 - Config Pre-delay Unit */

    retval &= vuprs::FPGA_API__PDLY__SetPredelay(&this->controller, this->predelayCount, this->channelName);

    /* FPGA: STEP 3 - Enable FIR */

    retval &= vuprs::FPGA_API__FIR__RuningControl(&this->controller, true);
    
    /* FPGA: STEP 4 - Update FIR coefficients with 0 */

    this->fir.GetZeroFIRBankCoefficient(&this->firCoefficients, this->bf_dcrcb.ElementCount());
    retval &= vuprs::FPGA_API__FIR__SetLengthAndCoefficients(&this->controller, &this->firCoefficients, 0.0, this->fir.FIRLength());

    /* FPGA: STEP 4 - Start ADC */

    retval &= vuprs::FPGA_API__ADC__StartADC(&this->controller, config.fs);

    this->beamformerStarted = retval;
    this->firstCoefficientsIssued = false;

    return retval;
}

bool vuprs::ARM_FPGA_CollaborationBeamfomer::BeamformerHasStarted() const
{
    return this->beamformerStarted;
}

bool vuprs::ARM_FPGA_CollaborationBeamfomer::GetSignalAndIssueFIRFilterCoefficients()
{
    if (!this->ConfigDone())
    {
        throw std::runtime_error("Config not complete.");
    }
    if (!this->BeamformerHasStarted())
    {
        std::runtime_error("Beam former not start.");
    }

    uint32_t RS;
    bool retval = true;

    /* Check if circular buffer refreshed */

    retval &= this->controller.dev__Circular_Buffer.ReadSingleRegister(vuprs::Circular_Buffer__Registers::CBUF_RS, &RS);

    if (!FPGA_REG_BIT(RS, 1)) return false;  /* not refreshed, return */

    /* --------------------------------------- Algorithm ------------------------------------------- */

    /* Read Circular Buffer */

    retval &= vuprs::FPGA_API__CBUF__ReadCircularBuffer(&this->controller, &this->multichannelSignal);

    /* Push data to Beam forming algorithm */

    this->bf_dcrcb.InputSignal(this->multichannelSignal);  /* Input signal */
    this->bf_dcrcb.UpdateCovarianceMatrix();  /* Update covariance matrix */

    if (!this->bf_dcrcb.CalculateEnable())
    {
        throw std::runtime_error("Beam forming algorithm cannot calculate.");
    }

    this->bf_dcrcb.CalculateBeamforming();  /* Calculate beam forming */
    this->bf_dcrcb.GetFIRExpectedFrequencyResponse(&this->firExpectedFrequencyResponse, true);  /* Get FIR filter bank expected frequency response */

    /* Convert frequency response to FIR coefficients */

    this->fir.SolveCoeffUseExpectedFrequencyResponse(this->firExpectedFrequencyResponse, this->hardwareSamplingFrequency);
    this->fir.GetFIRBankCoefficient(&this->firCoefficients);

    /* --------------------------------------- Hardware ------------------------------------------- */

    /* Write coefficients to FIT filter bank */

    retval &= vuprs::FPGA_API__FIR__SetCoefficients(&this->controller, &this->firCoefficients, this->fir.MaxAbsoluteFIRCoefficient());

    if (!this->firstCoefficientsIssued)
    {
        if (retval) this->firstCoefficientsIssued = true;
    }

    return retval;
}
