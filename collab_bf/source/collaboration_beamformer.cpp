#include "collaboration_beamformer.h"

#define ARM_FPGA_BF_COLLAB_CPP__DEBUG_PRINT false  /* print something @ debug mode */
#define ARM_FPGA_BF_COLLAB_CPP__DEBUG_SAVE false  /* save data @ debug mode */

vuprs::CollaborationBeamformer::CollaborationBeamformer()
{
    this->configdone = false;
    this->system_run = false;
    this->hardwareSamplingFrequency = 0.0;

    {
        std::lock_guard<std::mutex> lock(this->mut_scan_opt);  /* LOCK */
        this->scan_pointsInHalf = DEFAULT_SCANNING_POINTS_IN_HALF;
        this->scan_alt_min = DEFAULT_SCANNING_ALTITUDE_MIN;
        this->scan_waveVelocity = DEFAULT_SCANNING_WAVE_VELOCITY;
        vuprs::FibonacciGrid(this->scan_pointsInHalf, &this->scan_alt, &this->scan_az, this->scan_alt_min);
    }

    this->scanOptionsInitialized = false;

    this->BindBeamformer(std::make_unique<vuprs::Beamformer_DCRCB>());  /* default: DCRCB */
}

vuprs::CollaborationBeamformer::~CollaborationBeamformer()
{
    this->stop();
}

void vuprs::CollaborationBeamformer::BindBeamformer(std::unique_ptr<vuprs::WidebandBeamformerTemplate> beamformer)
{
    if (beamformer != nullptr)
    {
        this->bf = std::move(beamformer);
    }
}

bool vuprs::CollaborationBeamformer::ConfigDone() const
{
    return this->configdone;
}

void vuprs::CollaborationBeamformer::ScanSwitch(bool enable)
{
    this->scanEnable = enable;
}

bool vuprs::CollaborationBeamformer::ScanSwitch() const
{
    return this->scanEnable;
}

bool vuprs::CollaborationBeamformer::InitCollaborationBeamformer(const std::string &fpgaConfigJson, const std::string &bfArrayConfigJson, const std::string &firConfigJon)
{
    bool operateStatus = true;
    try
    {
        operateStatus &= this->controller.ConfigFPGAFromJson(fpgaConfigJson);
        operateStatus &= this->bf->ConfigArrayFromJson(bfArrayConfigJson);
        operateStatus &= this->fir.ConfigFIRFromJsonFile(firConfigJon);
    }
    catch(const std::exception& e)
    {
        throw std::runtime_error("in [CollaborationBeamformer::InitCollaborationBeamformer] Error occurred in initialization.");
    }

    this->configdone = operateStatus;
    return operateStatus;
}

void vuprs::CollaborationBeamformer::ScanOptions(int pointsInHalf, double alt_min, double waveVelocity)
{
    if (pointsInHalf <= 0)
    {
        throw std::runtime_error("in [CollaborationBeamformer::ScanOptions] pointsInHalf should be positive.");
    }
    if (alt_min < 0 || alt_min > 90)
    {
        throw std::runtime_error("in [CollaborationBeamformer::ScanOptions] alt_min should be between 0 and 90.");
    }
    if (waveVelocity <= 0)
    {
        throw std::runtime_error("in [CollaborationBeamformer::ScanOptions] waveVelocity should be positive.");
    }

    if (pointsInHalf != this->scan_pointsInHalf || alt_min != this->scan_alt_min || waveVelocity != this->scan_waveVelocity)
    {
        {
            std::lock_guard<std::mutex> lock(this->mut_scan_opt);  /* LOCK */

            this->scan_pointsInHalf = pointsInHalf;
            this->scan_alt_min = alt_min;
            vuprs::FibonacciGrid(this->scan_pointsInHalf, &this->scan_alt, &this->scan_az, this->scan_alt_min);

            this->scan_waveVelocity = waveVelocity;
        }
        this->scanOptionsChanged = true;
    }
}

bool vuprs::CollaborationBeamformer::ResetHardwareBeamformer()
{
    bool retval = true;

    /* FPGA reset */

    retval &= vuprs::FPGA_API__ADC__ResetADC(&this->controller);  /* Reset ADC controller */
    retval &= vuprs::FPGA_API__CBUF__ResetCircularBuffer(&this->controller);  /* Reset Circular Buffer */
    retval &= vuprs::FPGA_API__FIR__ResetFIR(&this->controller);  /* Reset FIR Filter Bank */
    retval &= vuprs::FPGA_API__DMA__ResetDMA(&this->controller);  /* Reset AXI DMA */

    /* Algorithm reset */

    {
        std::lock_guard<std::mutex> lock(this->mut_alg);
        this->bf->ResetCovarianceMatrices();
    }

    this->system_run = false;

    return retval;
}

bool vuprs::CollaborationBeamformer::StartBeamformerWithConfiguration(const CollaborationBeamformerConfig &config)
{
    if (!this->ConfigDone())
    {
        throw std::runtime_error("in [CollaborationBeamformer::StartBeamformerWithConfiguration] Config not complete.");
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
        std::lock_guard<std::mutex> lock(this->mut);  /* LOCK */

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
        std::lock_guard<std::mutex> lock(this->mut_alg);  /* LOCK */
        this->hardwareSamplingFrequency = this->controller.dev__ADC_Controller.SCI2FS(SCI);

        this->bf->ResetCovarianceMatrices();
        this->bf->SetCovarianceMatrixFittingParam(config.bf_cov_snapshotsWindowSize, config.bf_cov_freqAverageIndex);
        this->bf->SetTargetDirection(config.bf_target__alt, config.bf_target__az, config.bf_waveVelocity);

        /* Get predelay */

        this->bf->UpdateAndGetElementPredelay(this->fir.FIRLength(), this->hardwareSamplingFrequency, true,
            &predelayCount, &predelayTime, &channelName);

         /* Set frequency range */

        this->fir.SetFrequencyRange(config.bf_freq__lower, config.bf_freq__upper);
        this->fir.GetZeroFIRBankCoefficient(&firCoefficients, this->bf->ElementCount());
        FIR_LENGTH = this->fir.FIRLength();
    }

    /* System reset FPGA */

    retval &= this->ResetHardwareBeamformer();

    /* Set timeout for DMA interrupt (timeout = interrupt wait time / 100) */

    retval &= vuprs::FPGA_API_DMA__SetTimeoutForInterrupt(&this->controller, this->interruptWaitTime_us / (1000 * 100));

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
        throw std::runtime_error("in [CollaborationBeamformer::StartBeamformerWithConfiguration] Cannot start beam former with config");
    }
    
    return retval;
}

bool vuprs::CollaborationBeamformer::ReDirect(double alt, double az, double waveVelocity)
{
    std::vector<int> predelayCount;
    std::vector<double> predelayTime;
    std::vector<std::string> channelName;

    {
        std::lock_guard<std::mutex> lock(this->mut_alg);  /* LOCK */

        /* Set target direction */

        this->bf->SetTargetDirection(alt, az, waveVelocity);

        /* Get predelay */

        this->bf->UpdateAndGetElementPredelay(this->fir.FIRLength(), this->hardwareSamplingFrequency, true,
            &predelayCount, &predelayTime, &channelName);
    }

    return vuprs::FPGA_API__PDLY__SetPredelay(&this->controller, predelayCount, channelName);
}

bool vuprs::CollaborationBeamformer::isRun() const
{
    return this->system_run;
}

bool vuprs::CollaborationBeamformer::run(const vuprs::CollaborationBeamformerConfig &config)
{
    this->stop();
    this->StartBeamformerWithConfiguration(config);

    /* Start threads */

    this->system_run = true;

    this->threads.emplace_back([this](){this->THREAD__ReadResult();});
    this->threads.emplace_back([this](){this->THREAD__ListenDMAInterrupt();});
    this->threads.emplace_back([this](){this->THREAD__AlgorithmCalculation();});
    this->threads.emplace_back([this](){this->THREAD__ScanPowerCalculation();});
    this->threads.emplace_back([this](){this->THREAD__ReadCircularBuffer();});

    return true;
}

void vuprs::CollaborationBeamformer::stop()
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

bool vuprs::CollaborationBeamformer::HasResult() const
{
    return this->newResultDataInput;
}

bool vuprs::CollaborationBeamformer::ReadResult(std::vector<uint32_t> *result)
{
    bool readSuccess = false;

    if (this->newResultDataInput)
    {
        {
            std::lock_guard<std::mutex> lock(this->mut_dma);  /* LOCK */

            if (!this->resultQueue.empty())
            {
                *result = this->resultQueue.front();
                this->resultQueue.pop_front();
                readSuccess = true;
            }
            this->newResultDataInput = !this->resultQueue.empty();
        }
    }

    return readSuccess;
}

bool vuprs::CollaborationBeamformer::HasArraySignal() const
{
    return this->newArraySignalInput;
}

bool vuprs::CollaborationBeamformer::ReadArraySignal(vuprs::SignalData *signalData)
{
    bool readSuccess = false;

    if (this->newArraySignalInput)
    {
        {
            std::lock_guard<std::mutex> lock(this->mut_output_arraySignal);  /* LOCK */

            if (!this->outputArraySignalQueue.empty())
            {
                *signalData = this->outputArraySignalQueue.front();
                this->outputArraySignalQueue.pop_front();
                readSuccess = true;
            }
            this->newArraySignalInput = !this->outputArraySignalQueue.empty();
        }
    }

    return readSuccess;
}

bool vuprs::CollaborationBeamformer::HasScanPower() const
{
    return this->newScanPointsInput;
}

bool vuprs::CollaborationBeamformer::ReadScanPower(std::vector<uint16_t> *scanPower, double *maxPowerDB, double *minPowerDB)
{
    bool readSuccess = false;

    if (this->newScanPointsInput)
    {
        {
            std::lock_guard<std::mutex> lock(this->mut_scan_result);  /* LOCK */

            if (!this->scanResultQueue.empty())
            {
                *scanPower = this->scanResultQueue.front().scanResult;
                *maxPowerDB = this->scanResultQueue.front().maxPowerDB;
                *minPowerDB = this->scanResultQueue.front().minPowerDB;
                this->scanResultQueue.pop_front();
                readSuccess = true;
            }
            this->newScanPointsInput = !this->scanResultQueue.empty();
        }
    }

    return readSuccess;
}

void vuprs::CollaborationBeamformer::THREAD__ScanPowerCalculation()
{
    vuprs::ScanResult scanResult;
    std::vector<double> alt, az, _scanResult;
    double waveVelocity, powerRange;
    size_t pointsCount;
    uint16_t normalizedPower;
    bool calculateStatus = false;

    while (this->system_run)
    {
        {
            std::unique_lock<std::mutex> lock(this->mut_scan_result);  /* LOCK */
            this->scanCV.wait(lock, [this]() {
                return this->scanEnable || !this->system_run;
            });
        }

        if (!this->system_run) break;  /* Jump out */
        if (!this->scanEnable) continue;

        /* sync options */

        if (this->scanOptionsChanged || !this->scanOptionsInitialized)
        {
            std::lock_guard<std::mutex> lock(this->mut_scan_opt);  /* LOCK */
            alt = this->scan_alt;
            az = this->scan_az;
            waveVelocity = this->scan_waveVelocity;
            this->scanOptionsInitialized = true;
        }

        /* Calculate scan power */

        {
            std::lock_guard<std::mutex> lock(this->mut_alg);  /* LOCK */
            calculateStatus = this->bf->ScanForPositionPower(&_scanResult, &scanResult.maxPowerDB, &scanResult.minPowerDB, 
                alt, az, waveVelocity, 
                this->scanOptionsChanged, true);
        }
        if (!calculateStatus) continue;  /* If calculation failed, skip this loop */

        /* Convert to uint16_t */

        if (this->scanOptionsChanged) this->scanOptionsChanged = false;

        pointsCount = _scanResult.size();
        scanResult.scanResult.resize(pointsCount);
        powerRange = scanResult.maxPowerDB - scanResult.minPowerDB;

        for (size_t i = 0; i < pointsCount; i++)
        {
            normalizedPower = 0;
            if (powerRange > 1e-12)
            {
                double scaledPower = (_scanResult[i] - scanResult.minPowerDB) / powerRange * 65535.0;
                if (scaledPower < 0.0) scaledPower = 0.0;
                else if (scaledPower > 65535.0) scaledPower = 65535.0;
                normalizedPower = static_cast<uint16_t>(scaledPower);
            }
            scanResult.scanResult[i] = normalizedPower;  /* Convert to uint16_t safely */
        }

        /* Push data to dqueue */

        {
            std::lock_guard<std::mutex> lock(this->mut_scan_result);  /* LOCK */
            this->scanResultQueue.push_back(scanResult);
            if (this->scanResultQueue.size() > this->resultQueueSizeMAX)
            {
                this->scanResultQueue.pop_front();  /* Pop the oldest data to avoid overflow */
            }
        }
    }
}

void vuprs::CollaborationBeamformer::THREAD__ListenDMAInterrupt()
{
    uint32_t r_val;
    while (this->system_run)
    {
        try
        {
            vuprs::FPGA_API__DMA__GetAndClearInterruptFlag(&this->controller, &r_val);

            #if ARM_FPGA_BF_COLLAB_CPP__DEBUG_PRINT
                printf("[debug] DMA interrupt flag = %d\n", r_val);
            #endif
        }
        catch(const std::exception& e)
        {
            std::cout << "Error in [CollaborationBeamformer::THREAD__ListenDMAInterrupt] Error: " << e.what() << std::endl;
        }
        if (r_val == 0x01)
        {
            this->dmaDescriptorIRQ = true;
            this->dmaInterruptCV.notify_one();
        }
        if (!this->system_run) break;  /* Jump out */

        std::this_thread::sleep_for(std::chrono::microseconds(this->interruptWaitTime_us));
    }
}

void vuprs::CollaborationBeamformer::THREAD__ReadResult()
{
    vuprs::AXI_DMA_ScatterGatherDescriptor currentDescriptor, previousDescriptor, nextDescriptor;
    std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> _refDescriptors;
    vuprs::AlignedBufferDMA buffer;
    std::vector<uint32_t> result;
    bool hasInterrupt;

    {
        std::lock_guard<std::mutex> lock(this->mut);  /* LOCK */
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

            /* Convert buffer to vector */

            result = buffer.to_vector<uint32_t>();

            /* Push to queue */

            {
                std::lock_guard<std::mutex> lock(this->mut_dma);  /* LOCK */
                this->resultQueue.push_back(result);
                if (this->resultQueue.size() > this->resultQueueSizeMAX)
                {
                    this->resultQueue.pop_front();  /* Pop the oldest data to avoid overflow */
                }
            }

            this->newResultDataInput = true;
        }
        catch(const std::exception& e)
        {
            std::cout << "Error in [CollaborationBeamformer::THREAD__ReadResult] " << e.what() << std::endl;
        }
    }
}

void vuprs::CollaborationBeamformer::THREAD__ReadCircularBuffer()
{
    uint32_t r_val;
    vuprs::SignalData multichannelSignal;  /* signal data (from circular buffer) */

    while(this->system_run)
    {
        /* Check refreshed */

        try
        {
            this->controller.dev__Circular_Buffer.ReadSingleRegisterBIT(vuprs::Circular_Buffer__Registers::CBUF_RS, 1, &r_val);

            #if ARM_FPGA_BF_COLLAB_CPP__DEBUG_PRINT
                printf("[debug] circular buffer CBUF_RS[1] = %d\n", r_val);
            #endif
        }
        catch(const std::exception& e)
        {
            std::cout << "Error in [CollaborationBeamformer::THREAD__ReadCircularBuffer] " << e.what() << std::endl;
        }

        if (r_val == 0x01)
        {
            /* Read circular buffer */

            try
            {
                if (vuprs::FPGA_API__CBUF__ReadCircularBuffer(&this->controller, &multichannelSignal))
                {
                    {
                        std::lock_guard<std::mutex> lock(this->mut_alg);  /* LOCK */
                        this->arraySignalQueue.push_back(multichannelSignal);
                        if (this->arraySignalQueue.size() > this->circularBufferQueueSizeMAX)
                        {
                            this->arraySignalQueue.pop_front();  /* Pop the oldest data to avoid overflow */
                        }
                    }
                    this->circularBufferIRQ = true;
                    algorithmCV.notify_all();
                    {
                        std::lock_guard<std::mutex> lock(this->mut_output_arraySignal);  /* LOCK */
                        this->outputArraySignalQueue.push_back(multichannelSignal);
                        if (this->outputArraySignalQueue.size() > this->circularBufferQueueSizeMAX)
                        {
                            this->outputArraySignalQueue.pop_front();  /* Pop the oldest data to avoid overflow */
                        }
                    }
                    this->newArraySignalInput = true;
                }
                else
                {
                    throw std::runtime_error("in [CollaborationBeamformer::THREAD__ReadCircularBuffer] Cannot read circular buffer.");
                }
            }
            catch(const std::exception& e)
            {
                std::cout << "Error in [CollaborationBeamformer::THREAD__ReadCircularBuffer]" << e.what() << std::endl;
            }
        }

        if (!this->system_run) break;  /* Jump out */

        std::this_thread::sleep_for(std::chrono::microseconds(this->circularBufferWaitTime_us));
    }
}

void vuprs::CollaborationBeamformer::THREAD__AlgorithmCalculation()
{
    bool hasInterrupt, fpgaOperationStatus;
    vuprs::SignalData signal;
    Eigen::Matrix<Eigen::dcomplex, -1, -1> firExpectedFrequencyResponse;  /* Expected frequency response of FIR filter bank */
    std::vector<std::vector<double>> firCoefficients;  /* Coefficient of FIR filter bank */
    std::vector<std::string> channelName;  /* Channel name */

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
            std::lock_guard<std::mutex> lock(this->mut_alg);  /* LOCK */
            signal = this->arraySignalQueue.front();
            this->arraySignalQueue.pop_front();

            #if ARM_FPGA_BF_COLLAB_CPP__DEBUG_SAVE
                signal.ToCSV("../signals/signal.csv");
            #endif

            /* Push data to Beam forming algorithm */

            this->bf->InputSignal(signal);  /* Input signal */
            this->bf->UpdateCovarianceMatrix();  /* Update covariance matrix */

            if (this->scanEnable)
            {
                this->scanCV.notify_all();  /* Notify scan thread to calculate (if waiting) */
            }

            if (!this->bf->CalculateEnable())
            {
                throw std::runtime_error("in [CollaborationBeamformer::THREAD__AlgorithmCalculation] Beam forming algorithm cannot calculate.");
            }

            this->bf->CalculateBeamforming();  /* Calculate beam forming */
            this->bf->GetFIRExpectedFrequencyResponse(&firExpectedFrequencyResponse, &channelName, true);  /* Get FIR filter bank expected frequency response */

            /* Convert frequency response to FIR coefficients */

            this->fir.SolveCoeffUseExpectedFrequencyResponse(firExpectedFrequencyResponse, channelName, this->hardwareSamplingFrequency);
            this->fir.GetFIRBankCoefficient(&firCoefficients);
        }

        /* Issue coefficients to FIR */

        fpgaOperationStatus = true;

        try
        {
            fpgaOperationStatus &= vuprs::FPGA_API__FIR__SetCoefficients(&this->controller, &firCoefficients, this->fir.MaxAbsoluteFIRCoefficient());

            #if ARM_FPGA_BF_COLLAB_CPP__DEBUG_PRINT
                printf("[debug] max FIR coefficient = %.8f\n", this->fir.MaxAbsoluteFIRCoefficient());
            #endif

            if (!fpgaOperationStatus)
            {
                throw std::runtime_error("in [CollaborationBeamformer::THREAD__AlgorithmCalculation] FPGA operation failed.");
            }
        }
        catch(const std::exception& e)
        {
            std::cout << "Error in [CollaborationBeamformer::THREAD__AlgorithmCalculation] " << e.what() << std::endl;
        }
    }
}
