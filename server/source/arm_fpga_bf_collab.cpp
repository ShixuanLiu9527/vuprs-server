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
    config->queue__circularBufferQueueSizeMAX = 10;
    config->queue__resultQueueSizeMAX = 10;
}

bool vuprs::_Check_ARM_FPGA_BF_Config_Valid(vuprs::FPGAController *controller, const vuprs::ARM_FPGA_BF_Config &config)
{
    bool retval = true;

    retval &= (config.fs > 0 && config.fs < controller->dev__ADC_Controller.MaxSamplingFrequency());
    retval &= (config.bf_freq__lower < config.fs / 2.0);
    retval &= (config.bf_freq__upper < config.fs / 2.0);
    retval &= (config.bf_freq__lower < config.bf_freq__upper);
    retval &= (config.bf_cov_freqAverageIndex < 1.0);
    retval &= (config.dma__bufferSize % DMA_BUFFER_ALIGNMENT_1_WORD == 0);
    retval &= ((config.dma__bufferSize * config.dma__bufferCount) < controller->mem__DDR.MaxSizeBytes());

    return retval;
}

vuprs::ARM_FPGA_CollaborationBeamfomer::ARM_FPGA_CollaborationBeamfomer()
{
    this->configdone = false;
    this->system_run = false;
    this->hardwareSamplingFrequency = 0.0;
}

vuprs::ARM_FPGA_CollaborationBeamfomer::~ARM_FPGA_CollaborationBeamfomer()
{
    this->STOP();
}

bool vuprs::ARM_FPGA_CollaborationBeamfomer::ConfigDone() const
{
    return this->configdone;
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

bool vuprs::ARM_FPGA_CollaborationBeamfomer::ResetHardwareBeamformer()
{
    bool retval = true;

    /* FPGA reset */

    retval &= vuprs::FPGA_API__ADC__ResetADC(&this->controller);  /* Reset ADC controller */
    retval &= vuprs::FPGA_API__CBUF__ResetCircularBuffer(&this->controller);  /* Reset Circular Buffer */
    retval &= vuprs::FPGA_API__FIR__ResetFIR(&this->controller);  /* Reset FIR Filter Bank */
    retval &= vuprs::FPGA_API__DMA__ResetDMA(&this->controller);  /* Reset AXI DMA */

    /* Algorithm reset */

    {
        std::unique_lock<std::mutex> lock(this->mut_alg);
        this->bf_dcrcb.ResetAll();
    }

    this->system_run = false;

    return retval;
}

bool vuprs::ARM_FPGA_CollaborationBeamfomer::StartBeamformerWithConfiguration(const ARM_FPGA_BF_Config &config)
{
    if (!this->ConfigDone())
    {
        throw std::runtime_error("Config not complete.");
    }

    Eigen::Matrix<Eigen::dcomplex, -1, -1> firExpectedFrequencyResponse;  /* Expected frequency response of FIR filter bank */
    std::vector<std::vector<double>> firCoefficients;  /* Coefficient of FIR filter bank */
    std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> _dmaDescriptors;  /* SG descriptors for AXI DMA */

    std::vector<int> predelayCount;
    std::vector<double> predelayTime;
    std::vector<std::string> channelName;

    int descriptorUpdateCycle_us;
    uint32_t FIR_LENGTH;
    bool retval = true;

    /* max queue size */

    this->circularBufferQueueSizeMAX = config.queue__circularBufferQueueSizeMAX;
    this->resultQueueSizeMAX = config.queue__resultQueueSizeMAX;

    /* Generate descriptors */

    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */

        this->sg_descriptorConfig.bufferCount = config.dma__bufferCount;
        this->sg_descriptorConfig.bufferSize = config.dma__bufferSize;
        this->sg_descriptorConfig.ddr_FPGABaseAddr = this->controller.mem__DDR.FPGAAddress();
        this->sg_descriptorConfig.sgBRAM_FPGABaseAddr = this->controller.mem__SG_BRAM.FPGAAddress();

        this->sg_descriptorConfig.isCyclicDMAMode = true;

        vuprs::CreateDMAScatterGatherDescriptorChain(&this->dmaDescriptors, this->sg_descriptorConfig);
        _dmaDescriptors = this->dmaDescriptors;

        descriptorUpdateCycle_us = static_cast<int>(1000000 * static_cast<double>(config.dma__bufferSize) / config.fs);
    }

    this->interruptWaitTime_us = descriptorUpdateCycle_us / 10;
    this->circularBufferWaitTime_us = descriptorUpdateCycle_us / 5;

    /* Get sampling frequency */

    uint32_t SCI = this->controller.dev__ADC_Controller.GetSCIValueForSamplingFrequency(config.fs);

    {
        std::unique_lock<std::mutex> lock(this->mut_alg);  /* LOCK */
        this->hardwareSamplingFrequency = this->controller.dev__ADC_Controller.SCI2FS(SCI);

        this->bf_dcrcb.ResetCovarianceMatrices();
        this->bf_dcrcb.SetCovarianceMatrixFittingParam(config.bf_cov_snapshotsWindowSize, config.bf_cov_freqAverageIndex);
        this->bf_dcrcb.SetTargetDirection(config.bf_target__alt, config.bf_target__az, config.bf_waveVelocity);

        /* Get predelay */

        this->bf_dcrcb.UpdateAndGetElementPredelay(this->fir.FIRLength(), this->hardwareSamplingFrequency, 
            &predelayCount, &predelayTime, &channelName);

         /* Set frequency range */

        this->fir.SetFrequencyRange(config.bf_freq__lower, config.bf_freq__upper);
        this->fir.GetZeroFIRBankCoefficient(&firCoefficients, this->bf_dcrcb.ElementCount());
        FIR_LENGTH = this->fir.FIRLength();
    }

    /* System reset FPGA */

    retval &= this->ResetHardwareBeamformer();

    /* FPGA: STEP 1 - Config DMA */

    retval &= vuprs::FPGA_API__DMA__StartScatterGatherDMA_S2MM(&this->controller, _dmaDescriptors, true, true);

    /* FPGA: STEP 2 - Config Pre-delay Unit */

    retval &= vuprs::FPGA_API__PDLY__SetPredelay(&this->controller, predelayCount, channelName);

    /* FPGA: STEP 3 - Enable FIR */

    retval &= vuprs::FPGA_API__FIR__RuningControl(&this->controller, true);
    
    /* FPGA: STEP 4 - Update FIR coefficients with 0 */

    retval &= vuprs::FPGA_API__FIR__SetLengthAndCoefficients(&this->controller, &firCoefficients, 0.0, FIR_LENGTH);

    /* FPGA: STEP 4 - Start ADC */

    retval &= vuprs::FPGA_API__ADC__StartADC(&this->controller, config.fs);

    if (!retval)
    {
        throw std::runtime_error("Cannot start beam former with config");
    }
    
    return retval;
}

void vuprs::ARM_FPGA_CollaborationBeamfomer::ReDirect(double alt, double az, double waveVelocity)
{
    {
        std::unique_lock<std::mutex> lock(this->mut_alg);  /* LOCK */
        this->bf_dcrcb.SetTargetDirection(alt, az, waveVelocity);
    }
}

bool vuprs::ARM_FPGA_CollaborationBeamfomer::IS_RUN() const
{
    return this->system_run;
}

bool vuprs::ARM_FPGA_CollaborationBeamfomer::RUN(const vuprs::ARM_FPGA_BF_Config &config)
{
    this->STOP();
    this->StartBeamformerWithConfiguration(config);

    /* Start threads */

    this->system_run = true;

    this->threads.emplace_back([this](){this->THREAD__ReadResult();});
    this->threads.emplace_back([this](){this->THREAD__ListenDMAInterrupt();});
    this->threads.emplace_back([this](){this->THREAD__AlgorithmCalculation();});
    this->threads.emplace_back([this](){this->THREAD__ReadCircularBuffer();});

    return true;
}

void vuprs::ARM_FPGA_CollaborationBeamfomer::STOP()
{
    this->system_run = false;
    
    this->algorithmCV.notify_all();
    this->dmaInterruptCV.notify_all();

    for (auto &f: this->threads)
    {
        if (f.joinable())
        {
            f.join();
        }
    }
}

/* ------------------------------------------ Thread Control ----------------------------------------- */

bool vuprs::ARM_FPGA_CollaborationBeamfomer::NewResultDataInput() const
{
    return this->newResultDataInput;
}

bool vuprs::ARM_FPGA_CollaborationBeamfomer::ReadResultFromQueue(std::vector<double> *result)
{
    bool readSuccess = false;

    if (this->newResultDataInput)
    {
        this->newResultDataInput = false;
        {
            std::unique_lock<std::mutex> lock(this->mut_dma);  /* LOCK */

            if (!this->resultQueue.empty())
            {
                *result = this->resultQueue.front();
                this->resultQueue.pop();
                readSuccess = true;
            }
        }
    }

    return readSuccess;
}

void vuprs::ARM_FPGA_CollaborationBeamfomer::THREAD__ListenDMAInterrupt()
{
    uint32_t r_val;
    while (this->system_run)
    {
        try
        {
            vuprs::FPGA_API__DMA__GetAndClearInterruptFlag(&this->controller, &r_val);
        }
        catch(const std::exception& e)
        {
            std::cout << "Error: " << e.what() << std::endl;
        }
        if (r_val == 0x01)
        {
            this->dmaDescriptorIRQ = true;
            this->dmaInterruptCV.notify_one();
        }
        if (!this->system_run) break;  /* Jump out */
        usleep(this->interruptWaitTime_us);
    }
}

void vuprs::ARM_FPGA_CollaborationBeamfomer::THREAD__ReadResult()
{
    vuprs::AXI_DMA_ScatterGatherDescriptor currentDescriptor, previousDescriptor, nextDescriptor;
    vuprs::AlignedBufferDMA buffer;
    std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> _refDescriptors;
    bool hasInterrupt;

    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
        _refDescriptors = this->dmaDescriptors;
    }

    while(this->system_run)
    {
        /* Sleep here to wait interrupt */

        {
            std::unique_lock<std::mutex> lock(this->mut_dma);  /* LOCK */
            this->dmaInterruptCV.wait(lock, [this]() {
                return this->dmaDescriptorIRQ || !this->system_run;
            });
        }

        if (!this->system_run) break;  /* Jump out */

        if (this->dmaDescriptorIRQ) hasInterrupt = true;
        else hasInterrupt = false;

        this->dmaDescriptorIRQ = false;  /* Clear interrupt */

        if (!hasInterrupt) continue;

        /* Read DDR */
        
        try
        {
            /* Get previous descriptor */

            vuprs::FPGA_API__DMA__GetCurrentDescriptor(&this->controller, _refDescriptors, 
                &currentDescriptor, &previousDescriptor, &nextDescriptor);

            /* Read previous buffer (previous of current) */

            vuprs::FPGA_API__DDR__ReadDDR(&this->controller, &buffer, 
                previousDescriptor.BUFFER_ADDRESS, previousDescriptor.ALIGNMENT_2_BUFFER_SIZE);

            /* Push to queue */

            {
                std::unique_lock<std::mutex> lock(this->mut_dma);  /* LOCK */
                if (this->resultQueue.size() < this->resultQueueSizeMAX)
                {
                    this->resultQueue.push(buffer.to_vector<double>());
                }
            }

            this->newResultDataInput = true;
        }
        catch(const std::exception& e)
        {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }
}

void vuprs::ARM_FPGA_CollaborationBeamfomer::THREAD__ReadCircularBuffer()
{
    uint32_t r_val;
    vuprs::SignalData multichannelSignal;  /* signal data (from circular buffer) */

    while(this->system_run)
    {
        /* Check refreshed */

        try
        {
            this->controller.dev__Circular_Buffer.ReadSingleRegisterBIT(vuprs::Circular_Buffer__Registers::CBUF_RS, 1, &r_val);
        }
        catch(const std::exception& e)
        {
            std::cout << "Error: " << e.what() << std::endl;
        }

        if (r_val == 0x01)
        {
            /* Read circular buffer */

            try
            {
                if (vuprs::FPGA_API__CBUF__ReadCircularBuffer(&this->controller, &multichannelSignal))
                {
                    {
                        std::unique_lock<std::mutex> lock(this->mut_alg);  /* LOCK */
                        if (this->arraySignalQueue.size() < this->circularBufferQueueSizeMAX)
                        {
                            this->arraySignalQueue.push(multichannelSignal);
                        }
                    }
                    this->circularBufferIRQ = true;
                    algorithmCV.notify_all();
                }
                else
                {
                    throw std::runtime_error("Cannot read circular buffer.");
                }
            }
            catch(const std::exception& e)
            {
                std::cout << "Error: " << e.what() << std::endl;
            }
        }

        if (!this->system_run) break;  /* Jump out */
        usleep(this->circularBufferWaitTime_us);
    }
}

void vuprs::ARM_FPGA_CollaborationBeamfomer::THREAD__AlgorithmCalculation()
{
    bool hasInterrupt, fpgaOperationStatus;
    vuprs::SignalData signal;
    Eigen::Matrix<Eigen::dcomplex, -1, -1> firExpectedFrequencyResponse;  /* Expected frequency response of FIR filter bank */
    std::vector<std::vector<double>> firCoefficients;  /* Coefficient of FIR filter bank */

    while(this->system_run)
    {
        /* Sleep here to wait interrupt */

        {
            std::unique_lock<std::mutex> lock(this->mut_alg);  /* LOCK */
            this->algorithmCV.wait(lock, [this]() {
                return this->circularBufferIRQ || !this->system_run;
            });
        }

        /* Check interrupt flag */

        if (!this->system_run) break;  /* Jump out */

        if (this->circularBufferIRQ) hasInterrupt = true;
        else hasInterrupt = false;

        this->circularBufferIRQ = false;

        if (!hasInterrupt) continue;

        /* Do algorithm calculation */

        {
            std::unique_lock<std::mutex> lock(this->mut_alg);  /* LOCK */
            signal = this->arraySignalQueue.front();
            this->arraySignalQueue.pop();

            /* Push data to Beam forming algorithm */

            this->bf_dcrcb.InputSignal(signal);  /* Input signal */
            this->bf_dcrcb.UpdateCovarianceMatrix();  /* Update covariance matrix */

            if (!this->bf_dcrcb.CalculateEnable())
            {
                throw std::runtime_error("Beam forming algorithm cannot calculate.");
            }

            this->bf_dcrcb.CalculateBeamforming();  /* Calculate beam forming */
            this->bf_dcrcb.GetFIRExpectedFrequencyResponse(&firExpectedFrequencyResponse, true);  /* Get FIR filter bank expected frequency response */

            /* Convert frequency response to FIR coefficients */

            this->fir.SolveCoeffUseExpectedFrequencyResponse(firExpectedFrequencyResponse, this->hardwareSamplingFrequency);
            this->fir.GetFIRBankCoefficient(&firCoefficients);
        }

        /* Issue coefficients to FIR */

        fpgaOperationStatus = true;

        try
        {
            fpgaOperationStatus &= vuprs::FPGA_API__FIR__SetCoefficients(&this->controller, 
                &firCoefficients, this->fir.MaxAbsoluteFIRCoefficient());
            if (!fpgaOperationStatus)
            {
                throw std::runtime_error("FPGA operation failed.");
            }
        }
        catch(const std::exception& e)
        {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }
}
