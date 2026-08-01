#include "config.h"
#include "hybrid/hybrid_bf.h"
#include "logger/check.h"

vuprs::HybridBeamformer::HybridBeamformer()
{
    this->config_done = false;
    this->system_run = false;
    this->hardware_fs = 0.0;
    {
        std::lock_guard<std::mutex> lock(this->mut_scan_opt); /* LOCK */
        this->scan_points_in_hemisphere = DEFAULT_SCANNING_POINTS_IN_HALF;
        this->scan_alt_min = DEFAULT_SCANNING_ALTITUDE_MIN;
        this->scan_wave_velocity = DEFAULT_WAVE_VELOCITY;
        vuprs::FibonacciGrid(this->scan_points_in_hemisphere,
                             &this->scan_alt,
                             &this->scan_az,
                             this->scan_alt_min);
    }
    this->scan_options_initialized = false;
    this->BindBeamformer(std::make_unique<vuprs::Beamformer_DCRCB>()); /* default: DCRCB */
}

vuprs::HybridBeamformer::~HybridBeamformer()
{
    this->stop();
}

void vuprs::HybridBeamformer::BindBeamformer(std::unique_ptr<vuprs::WidebandBeamformerTemplate> beamformer)
{
    if (beamformer != nullptr)
    {
        this->bf = std::move(beamformer);
    }
}

bool vuprs::HybridBeamformer::ConfigDone() const
{
    return this->config_done;
}

void vuprs::HybridBeamformer::ScanSwitch(bool enable)
{
    this->scan_enable = enable;
}

bool vuprs::HybridBeamformer::ScanSwitch() const
{
    return this->scan_enable;
}

bool vuprs::HybridBeamformer::InitCollaborationBeamformer(const std::string &fpga_config_json, const std::string &bf_array_config_json, const std::string &fir_config_json)
{
    bool operate_status = true;
    try
    {
        operate_status &= this->controller.ConfigFPGAFromJson(fpga_config_json);
        operate_status &= this->bf->ConfigArrayFromJson(bf_array_config_json);
        operate_status &= this->fir.ConfigFIRFromJsonFile(fir_config_json);
    }
    catch (const std::exception &e)
    {
        RUNTIME_CHECK(false, "hybrid_bf", " in [HybridBeamformer::InitCollaborationBeamformer] Error occurred in initialization.");
    }

    this->config_done = operate_status;
    return operate_status;
}

void vuprs::HybridBeamformer::ScanOptions(int points_in_hemisphere, double alt_min, double wave_velocity)
{
    PARAM_CHECK(points_in_hemisphere > 0, "hybrid_bf", " in [HybridBeamformer::ScanOptions] points_in_hemisphere should be positive.");
    PARAM_CHECK(alt_min >= 0 && alt_min <= 90, "hybrid_bf", " in [HybridBeamformer::ScanOptions] alt_min should be between 0 and 90.");
    PARAM_CHECK(wave_velocity > 0, "hybrid_bf", " in [HybridBeamformer::ScanOptions] wave_velocity should be positive.");
    if (points_in_hemisphere != this->scan_points_in_hemisphere || alt_min != this->scan_alt_min || wave_velocity != this->scan_wave_velocity)
    {
        {
            std::lock_guard<std::mutex> lock(this->mut_scan_opt); /* LOCK */

            this->scan_points_in_hemisphere = points_in_hemisphere;
            this->scan_alt_min = alt_min;
            vuprs::FibonacciGrid(this->scan_points_in_hemisphere,
                                 &this->scan_alt,
                                 &this->scan_az,
                                 this->scan_alt_min);
            this->scan_wave_velocity = wave_velocity;
        }
        this->scan_options_changed = true;
    }
}

bool vuprs::HybridBeamformer::ResetHardwareBeamformer()
{
    bool retval = true;
    /* FPGA reset */
    retval &= vuprs::FPGA_API__ADC__ResetADC(&this->controller);             /* Reset ADC controller */
    retval &= vuprs::FPGA_API__CBUF__ResetCircularBuffer(&this->controller); /* Reset Circular Buffer */
    retval &= vuprs::FPGA_API__FIR__ResetFIR(&this->controller);             /* Reset FIR Filter Bank */
    retval &= vuprs::FPGA_API__DMA__ResetDMA(&this->controller);             /* Reset AXI DMA */
    this->system_run = false;
    return retval;
}

bool vuprs::HybridBeamformer::StartBeamformerWithConfiguration(const HybridBeamformerConfig &config)
{
    PARAM_CHECK(this->ConfigDone(), "hybrid_bf", " in [HybridBeamformer::StartBeamformerWithConfiguration] Config not complete.");
    Eigen::Matrix<Eigen::dcomplex, -1, -1> fir_expected_frequency_response; /* Expected frequency response of FIR filter bank */
    std::vector<std::vector<double>> fir_coefficients;                      /* Coefficient of FIR filter bank */
    std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> _dma_descriptors;   /* SG descriptors for AXI DMA */
    std::vector<int> predelay_count;
    std::vector<double> predelay_time;
    std::vector<std::string> channel_name;
    int descriptor_update_cycle_us;
    uint32_t FIR_LENGTH;
    bool retval = true;

    /* Max queue size */
    this->circular_buffer_queue_size_max = config.queue__circular_buffer_queue_size_max;
    this->result_queue_size_max = config.queue__result_queue_size_max;
    /* Step 1: Generate descriptors */
    {
        std::lock_guard<std::mutex> lock(this->mut); /* LOCK */
        this->sg_descriptor_config.buffer_count = config.dma__buffer_count;
        this->sg_descriptor_config.buffer_size = config.dma__buffer_size;
        this->sg_descriptor_config.ddr_fpga_base_addr = this->controller.mem__ddr.FPGAAddress();
        this->sg_descriptor_config.sg_bram_fpga_base_addr = this->controller.mem__sg_bram.FPGAAddress();
        this->sg_descriptor_config.is_cyclic_dma_mode = true;
        vuprs::CreateDMAScatterGatherDescriptorChain(&this->dma_descriptors, this->sg_descriptor_config);
        _dma_descriptors = this->dma_descriptors;
        descriptor_update_cycle_us = static_cast<int>(1000000 * static_cast<double>(config.dma__buffer_size) / config.fs);
    }
    /* Step 2: Initialize loop cycle */
    this->interrupt_wait_time_us = descriptor_update_cycle_us / 20;
    this->circular_buffer_wait_time_us = descriptor_update_cycle_us / 20;
#if DEBUG
    printf("DMA descriptor update cycle: %d us\n", descriptor_update_cycle_us);
    printf("DMA interrupt wait time: %d us\n", this->interrupt_wait_time_us.load());
    printf("Circular buffer interrupt wait time: %d us\n", this->circular_buffer_wait_time_us.load());
#endif
    /* Step 3: Get sampling frequency register (SCI) */
    uint32_t SCI = this->controller.dev__adc_controller.GetSCIValueForSamplingFrequency(config.fs);
    /* Step 4: Reset and initialize algorithm obj */
    {
        std::lock_guard<std::mutex> lock(this->mut_alg); /* LOCK */
        /* - Hardware sampling frequency */
        this->hardware_fs = this->controller.dev__adc_controller.SCI2FS(SCI);
        /* - Reset algorithm obj */
        this->bf->ResetCovarianceMatrices();
        /* - Set covariance matrix fitting parameters */
        this->bf->SetCovarianceMatrixFittingParam(config.bf_cov_snapshots_window_size, config.bf_cov_freq_average_index);
        /* - Set beamformer pointing position */
        this->bf->SetTargetDirection(config.bf_target__alt,
                                     config.bf_target__az,
                                     config.bf_wave_velocity);
        /* - Get predelay */
        this->bf->UpdateAndGetElementPredelay(this->fir.FIRLength(),
                                              this->hardware_fs,
                                              true,
                                              &predelay_count,
                                              &predelay_time,
                                              &channel_name);
        /* - Set bandpass range */
        this->fir.SetFrequencyRange(config.bf_freq__lower, config.bf_freq__upper);
        /* - Get zero FIR coefficients */
        this->fir.GetZeroFIRBankCoefficient(&fir_coefficients, this->bf->ElementCount());
        FIR_LENGTH = this->fir.FIRLength();
    }
    /* Step 5: System reset FPGA */
    retval &= this->ResetHardwareBeamformer();
    /* Step 6: Config FPGA */
    /* - FPGA config step 1 - Config DMA */
    retval &= vuprs::FPGA_API__DMA__StartScatterGatherDMA_S2MM(&this->controller,
                                                               _dma_descriptors,
                                                               true,
                                                               true);
    /* - FPGA config step 2 - Config Pre-delay Unit */
    retval &= vuprs::FPGA_API__PDLY__SetPredelay(&this->controller,
                                                 predelay_count,
                                                 channel_name);
    /* - FPGA config step 3 - Enable FIR */
    retval &= vuprs::FPGA_API__FIR__RunningControl(&this->controller, true);
    /* - FPGA config step 4 - Update FIR coefficients with 0 */
    retval &= vuprs::FPGA_API__FIR__SetLengthAndCoefficients(&this->controller,
                                                             &fir_coefficients,
                                                             0.0,
                                                             FIR_LENGTH);
    /* - FPGA config step 5 - Start ADC */
    retval &= vuprs::FPGA_API__ADC__StartADC(&this->controller, config.fs);
    RUNTIME_CHECK(retval, "hybrid_bf", " in [HybridBeamformer::StartBeamformerWithConfiguration] Cannot start beam former with config");
    return retval;
}

bool vuprs::HybridBeamformer::ReDirect(double alt, double az, double wave_velocity)
{
    std::vector<int> predelay_count;
    std::vector<double> predelay_time;
    std::vector<std::string> channel_name;
    {
        std::lock_guard<std::mutex> lock(this->mut_alg); /* LOCK */
        /* Set target direction */
        this->bf->SetTargetDirection(alt, az, wave_velocity);
        /* Get predelay */
        this->bf->UpdateAndGetElementPredelay(this->fir.FIRLength(),
                                              this->hardware_fs,
                                              true,
                                              &predelay_count,
                                              &predelay_time,
                                              &channel_name);
    }
    return vuprs::FPGA_API__PDLY__SetPredelay(&this->controller,
                                              predelay_count,
                                              channel_name);
}

bool vuprs::HybridBeamformer::isRun() const
{
    return this->system_run;
}

bool vuprs::HybridBeamformer::run(const vuprs::HybridBeamformerConfig &config)
{
    this->stop();
    this->StartBeamformerWithConfiguration(config);
    /* Start threads */
    this->system_run = true;
    this->threads.emplace_back([this]()
                               { this->THREAD__ReadResult(); });
    this->threads.emplace_back([this]()
                               { this->THREAD__ListenDMAInterrupt(); });
    this->threads.emplace_back([this]()
                               { this->THREAD__AlgorithmCalculation(); });
    this->threads.emplace_back([this]()
                               { this->THREAD__ScanPowerCalculation(); });
    this->threads.emplace_back([this]()
                               { this->THREAD__ReadCircularBuffer(); });
    return true;
}

void vuprs::HybridBeamformer::stop()
{
    this->system_run = false;
    this->algorithm_cv.notify_all();
    this->dma_interrupt_cv.notify_all();
    this->scan_cv.notify_all();
    for (auto &f : this->threads)
    {
        if (f.joinable())
        {
            f.join();
        }
    }
    this->ResetHardwareBeamformer();
}

/* ------------------------------------------ Thread Control ----------------------------------------- */

bool vuprs::HybridBeamformer::HasResult() const
{
    return this->new_result_data_input;
}

bool vuprs::HybridBeamformer::ReadResult(std::vector<uint32_t> *result)
{
    bool read_success = false;
    if (this->new_result_data_input)
    {
        {
            std::lock_guard<std::mutex> lock(this->mut_dma); /* LOCK */
            if (!this->result_queue.empty())
            {
                *result = this->result_queue.front();
                this->result_queue.pop_front();
                read_success = true;
            }
            this->new_result_data_input = !this->result_queue.empty();
        }
    }
    return read_success;
}

bool vuprs::HybridBeamformer::HasArraySignal() const
{
    return this->new_array_signal_input;
}

bool vuprs::HybridBeamformer::ReadArraySignal(vuprs::SignalData *signalData)
{
    bool read_success = false;
    if (this->new_array_signal_input)
    {
        {
            std::lock_guard<std::mutex> lock(this->mut_output_arraySignal); /* LOCK */
            if (!this->output_array_signal_queue.empty())
            {
                *signalData = this->output_array_signal_queue.front();
                this->output_array_signal_queue.pop_front();
                read_success = true;
            }
            this->new_array_signal_input = !this->output_array_signal_queue.empty();
        }
    }
    return read_success;
}

bool vuprs::HybridBeamformer::HasScanPower() const
{
    return this->new_scan_points_input;
}

bool vuprs::HybridBeamformer::ReadScanPower(std::vector<uint16_t> *scanPower, double *max_power_db, double *min_power_db)
{
    bool read_success = false;
    if (this->new_scan_points_input)
    {
        {
            std::lock_guard<std::mutex> lock(this->mut_scan_result); /* LOCK */
            if (!this->scan_result_queue.empty())
            {
                *scanPower = this->scan_result_queue.front().scan_result;
                *max_power_db = this->scan_result_queue.front().max_power_db;
                *min_power_db = this->scan_result_queue.front().min_power_db;
                this->scan_result_queue.pop_front();
                read_success = true;
            }
            this->new_scan_points_input = !this->scan_result_queue.empty();
        }
    }
    return read_success;
}

void vuprs::HybridBeamformer::THREAD__ScanPowerCalculation()
{
    vuprs::ScanResult scan_result;
    std::vector<double> alt, az, _scan_result;
    double wave_velocity, power_range;
    size_t points_count;
    uint16_t normalized_power;
    bool calculate_status = false;
    while (this->system_run)
    {
        {
            std::unique_lock<std::mutex> lock(this->mut_scan_result); /* LOCK */
            this->scan_cv.wait(lock, [this]()
                               { return this->scan_enable || !this->system_run; });
        }
        if (!this->system_run)
            break; /* Jump out */
        if (!this->scan_enable)
            continue;
        /* sync options */
        if (this->scan_options_changed || !this->scan_options_initialized)
        {
            std::lock_guard<std::mutex> lock(this->mut_scan_opt); /* LOCK */
            alt = this->scan_alt;
            az = this->scan_az;
            wave_velocity = this->scan_wave_velocity;
            this->scan_options_initialized = true;
        }
        /* Calculate scan power */
        {
            std::lock_guard<std::mutex> lock(this->mut_alg); /* LOCK */
            calculate_status = this->bf->ScanForPositionPower(&_scan_result, &scan_result.max_power_db, &scan_result.min_power_db,
                                                              alt, az, wave_velocity,
                                                              this->scan_options_changed, true);
        }
        if (!calculate_status)
            continue; /* If calculation failed, skip this loop */
        /* Convert to uint16_t */
        if (this->scan_options_changed)
            this->scan_options_changed = false;
        points_count = _scan_result.size();
        scan_result.scan_result.resize(points_count);
        power_range = scan_result.max_power_db - scan_result.min_power_db;
        for (size_t i = 0; i < points_count; i++)
        {
            normalized_power = 0;
            if (power_range > 1e-12)
            {
                double scaled_power = (_scan_result[i] - scan_result.min_power_db) / power_range * 65535.0;
                if (scaled_power < 0.0)
                    scaled_power = 0.0;
                else if (scaled_power > 65535.0)
                    scaled_power = 65535.0;
                normalized_power = static_cast<uint16_t>(scaled_power);
            }
            scan_result.scan_result[i] = normalized_power; /* Convert to uint16_t safely */
        }
        /* Push data to dqueue */
        {
            std::lock_guard<std::mutex> lock(this->mut_scan_result); /* LOCK */
            this->scan_result_queue.push_back(scan_result);
            if (this->scan_result_queue.size() > this->result_queue_size_max)
            {
                this->scan_result_queue.pop_front(); /* Pop the oldest data to avoid overflow */
            }
        }
        this->new_scan_points_input = true;
    }
}

void vuprs::HybridBeamformer::THREAD__ListenDMAInterrupt()
{
    uint32_t r_val;
    bool is_first_change = true;
    while (this->system_run)
    {
        try
        {
            vuprs::FPGA_API__DMA__ReadCurrentDescriptor(&this->controller, &r_val);
            if (r_val != this->dma_current_desc.load())
            {
                this->dma_current_desc.store(r_val);
                if (!is_first_change)
                {
#if DEBUG
                    printf("[debug] DMA interrupt detected, current desc = 0x%X\n", r_val);
#endif
                    this->dma_descriptor_irq = true;
                    this->dma_interrupt_cv.notify_one();
                }
                else
                {
                    is_first_change = false; /* Skip the first change since it's just the initial value */
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cout << "Error in [HybridBeamformer::THREAD__ListenDMAInterrupt] Error: " << e.what() << std::endl;
        }
        if (!this->system_run)
            break; /* Jump out */
        std::this_thread::sleep_for(std::chrono::microseconds(this->interrupt_wait_time_us));
    }
}

void vuprs::HybridBeamformer::THREAD__ReadResult()
{
    vuprs::AXI_DMA_ScatterGatherDescriptor current_descriptor, previous_descriptor, next_descriptor;
    std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> _ref_descriptors;
    vuprs::AlignedBufferDMA buffer;
    std::vector<uint32_t> result;
    bool has_interrupt;
#if DEBUG
    std::vector<double> result_d;
    int debug_file_group = 0;
#endif
    {
        std::lock_guard<std::mutex> lock(this->mut); /* LOCK */
        _ref_descriptors = this->dma_descriptors;
    }
    while (this->system_run)
    {
        /* Sleep here to wait interrupt */

        {
            std::unique_lock<std::mutex> lock(this->mut_dma); /* LOCK */
            this->dma_interrupt_cv.wait(lock,
                                        [this]()
                                        { return this->dma_descriptor_irq || !this->system_run; });
        }
        if (!this->system_run)
            break; /* Jump out */
        if (this->dma_descriptor_irq)
            has_interrupt = true;
        else
            has_interrupt = false;
        this->dma_descriptor_irq = false; /* Clear interrupt */
        if (!has_interrupt)
            continue;
        /* Get previous descriptor */
        if (vuprs::MatchDescriptor(_ref_descriptors,
                                   this->dma_current_desc.load(),
                                   &current_descriptor,
                                   &previous_descriptor,
                                   &next_descriptor))
        {
            try
            {
                /* Read previous buffer (previous of current) from DDR */
                vuprs::FPGA_API__DDR__ReadDDR(&this->controller,
                                              &buffer,
                                              previous_descriptor.BUFFER_ADDRESS,
                                              previous_descriptor.ALIGNMENT_2_BUFFER_SIZE);
                /* Convert buffer to vector */
                result = buffer.to_vector<uint32_t>();
#if DEBUG
                buffer.to_file(std::string(DEBUG_FILES_ROOT_DIR) + "/" +
                               std::string(DEBUG_FILES_DIR) + "-" + std::to_string(debug_file_group) + "/" +
                               std::string(FIR_RESULT_BIN_DEBUG_FILENAME));
                vuprs::FIRResult_Q16_TO_DOUBLE(result, &result_d); /* Convert to double */
                vuprs::SaveToCSV(result_d, std::string(DEBUG_FILES_ROOT_DIR) + "/" +
                                               std::string(DEBUG_FILES_DIR) + "-" + std::to_string(debug_file_group) + "/" +
                                               std::string(FIR_RESULT_DEBUG_FILENAME));
                debug_file_group++;
                if (debug_file_group >= DEBUG_DATA_GROUP_COUNT)
                    debug_file_group = 0;
#endif
                /* Push data to queue */
                {
                    std::lock_guard<std::mutex> lock(this->mut_dma); /* LOCK */
                    this->result_queue.push_back(result);
                    if (this->result_queue.size() > this->result_queue_size_max)
                    {
                        this->result_queue.pop_front(); /* Pop the oldest data to avoid overflow */
                    }
                }
                this->new_result_data_input = true;
            }
            catch (const std::exception &e)
            {
                std::cout << "Error in [HybridBeamformer::THREAD__ReadResult] " << e.what() << std::endl;
            }
        }
        else
        {
            std::cout << "Warning in [HybridBeamformer::THREAD__ReadResult] Cannot match current descriptor address to any in the reference list." << std::endl;
        }
    }
}

void vuprs::HybridBeamformer::THREAD__ReadCircularBuffer()
{
    uint32_t r_val;
    vuprs::SignalData multi_channel_signal; /* signal data (from circular buffer) */
    while (this->system_run)
    {
        /* Check refreshed */
        try
        {
            this->controller.dev__circular_buffer.ReadSingleRegisterBIT(vuprs::Circular_Buffer__Registers::CBUF_RS, 1, &r_val);
#if DEBUG
            printf("[debug] circular buffer CBUF_RS[1] = %d\n", r_val);
#endif
        }
        catch (const std::exception &e)
        {
            std::cout << "Error in [HybridBeamformer::THREAD__ReadCircularBuffer] " << e.what() << std::endl;
        }
        if (r_val == 0x01)
        {
            /* Read circular buffer */
            try
            {
                if (vuprs::FPGA_API__CBUF__ReadCircularBuffer(&this->controller, &multi_channel_signal))
                {
                    {
                        std::lock_guard<std::mutex> lock(this->mut_alg); /* LOCK */
                        this->array_signal_queue.push_back(multi_channel_signal);
                        if (this->array_signal_queue.size() > this->circular_buffer_queue_size_max)
                        {
                            this->array_signal_queue.pop_front(); /* Pop the oldest data to avoid overflow */
                        }
                    }
                    this->circular_buffer_irq = true;
                    algorithm_cv.notify_all();
                    {
                        std::lock_guard<std::mutex> lock(this->mut_output_arraySignal); /* LOCK */
                        this->output_array_signal_queue.push_back(multi_channel_signal);
                        if (this->output_array_signal_queue.size() > this->circular_buffer_queue_size_max)
                        {
                            this->output_array_signal_queue.pop_front(); /* Pop the oldest data to avoid overflow */
                        }
                    }
                    this->new_array_signal_input = true;
                }
                else
                {
                    RUNTIME_CHECK(false, "hybrid_bf", " in [HybridBeamformer::THREAD__ReadCircularBuffer] Cannot read circular buffer.");
                }
            }
            catch (const std::exception &e)
            {
                std::cout << "Error in [HybridBeamformer::THREAD__ReadCircularBuffer]" << e.what() << std::endl;
            }
        }
        if (!this->system_run)
            break; /* Jump out */
        std::this_thread::sleep_for(std::chrono::microseconds(this->circular_buffer_wait_time_us));
    }
}

void vuprs::HybridBeamformer::THREAD__AlgorithmCalculation()
{
    bool has_interrupt, operation - status;
    vuprs::SignalData signal;
    Eigen::Matrix<Eigen::dcomplex, -1, -1> fir_expected_frequency_response; /* Expected frequency response of FIR filter bank */
    std::vector<std::vector<double>> fir_coefficients;                      /* Coefficient of FIR filter bank, [channel][point] */
    std::vector<std::string> channel_name;                                  /* Channel name */
#if DEBUG
    int debug_file_group = 0;
#endif
    while (this->system_run)
    {
        /* Sleep here to wait interrupt */
        {
            std::unique_lock<std::mutex> lock(this->mut_alg); /* LOCK */
            this->algorithm_cv.wait(lock, [this]()
                                    { return this->circular_buffer_irq || !this->system_run; });
        }
        /* Check interrupt flag */
        if (!this->system_run)
            break; /* Jump out */
        if (this->circular_buffer_irq)
            has_interrupt = true;
        else
            has_interrupt = false;
        this->circular_buffer_irq = false;
        if (!has_interrupt)
            continue;
        /* Do algorithm calculation */
        {
            std::lock_guard<std::mutex> lock(this->mut_alg); /* LOCK */
            /* Read signal data from queue */
            signal = this->array_signal_queue.front();
            this->array_signal_queue.pop_front();
            /* Step 1: Push data to Beam forming algorithm */
            this->bf->InputSignal(signal);
            /* Step 2: Update covariance matrix */
            this->bf->UpdateCovarianceMatrix();
            if (this->scan_enable)
            {
                this->scan_cv.notify_all(); /* Notify scan thread to calculate (if waiting) */
            }
            /* - Check calculate enabled */
            RUNTIME_CHECK(this->bf->CalculateEnable(), "hybrid_bf", " in [HybridBeamformer::THREAD__AlgorithmCalculation] Beam forming algorithm cannot calculate.");
            /* Step 3: Calculate beamforming */
            this->bf->CalculateBeamforming();
            /* Step 4: Get FIR filter bank expected frequency response */
            this->bf->GetFIRExpectedFrequencyResponse(&fir_expected_frequency_response,
                                                      &channel_name,
                                                      true);
            /* Step 5: Convert frequency response to FIR coefficients */
            this->fir.SolveCoeffUseExpectedFrequencyResponse(fir_expected_frequency_response,
                                                             channel_name,
                                                             this->hardware_fs);
            this->fir.GetFIRBankCoefficient(&fir_coefficients);
        }
#if DEBUG
        signal.ToCSV(std::string(DEBUG_FILES_ROOT_DIR) + "/" +
                     std::string(DEBUG_FILES_DIR) + "-" + std::to_string(debug_file_group) + "/" +
                     std::string(CIRCULAR_BUFFER_DEBUG_FILENAME));
        printf("[debug] max FIR coefficient = %.8f\n", this->fir.MaxAbsoluteFIRCoefficient());
        debug_file_group++;
        if (debug_file_group >= DEBUG_DATA_GROUP_COUNT)
            debug_file_group = 0;
#endif
        /* Issue coefficients to FIR */
        operation - status = true;
        try
        {
            operation - status &= vuprs::FPGA_API__FIR__SetCoefficients(&this->controller,
                                                                        &fir_coefficients,
                                                                        this->fir.MaxAbsoluteFIRCoefficient());
            RUNTIME_CHECK(operation - status, "hybrid_bf", "FPGA operation failed.");
        }
        catch (const std::exception &e)
        {
            std::cout << "Error in [HybridBeamformer::THREAD__AlgorithmCalculation] " << e.what() << std::endl;
        }
    }
}
