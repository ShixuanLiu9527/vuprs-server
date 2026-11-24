#include <cmath>
#include "config.h"
#include "hybrid/hybrid_bf.h"
#include "logger/check.h"

#define HYBRID_LOG(level) _LOG(this->hybrid_logger, level)

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
                             this->scan_alt_min,
                             &this->scan_alt,
                             &this->scan_az);
    }
    this->scan_options_initialized = false;
    this->BindBeamformer(std::make_unique<vuprs::Beamformer_DCRCB>()); /* default: DCRCB */
}

vuprs::HybridBeamformer::~HybridBeamformer()
{
    try
    {
        this->stop();
    }
    catch (...)
    {
    }
}

bool vuprs::HybridBeamformer::BindBeamformer(std::unique_ptr<vuprs::WidebandBeamformerTemplate> beamformer)
{
    if (beamformer != nullptr)
    {
        this->bf = std::move(beamformer);
    }
    return beamformer != nullptr;
}

bool vuprs::HybridBeamformer::ConfigDone() const
{
    return this->config_done;
}

void vuprs::HybridBeamformer::SetResultCallback(std::function<void(BeamformerResultMeta &&)> callback)
{
    this->result_callback_ = std::move(callback);
}

void vuprs::HybridBeamformer::SetScanCallback(std::function<void(ScanResult &&)> callback)
{
    this->scan_callback_ = std::move(callback);
}

void vuprs::HybridBeamformer::SetConsumerGate(std::function<bool()> gate)
{
    this->consumer_gate_ = std::move(gate);
}

double vuprs::HybridBeamformer::HardwareSamplingFrequency()
{
    return this->hardware_fs.load();
}

double vuprs::HybridBeamformer::MaxSamplingFrequency() const
{
    return this->controller.dev__adc_controller.MaxSamplingFrequency();
}

uint32_t vuprs::HybridBeamformer::SignalPoints() const
{
    return this->controller.dev__circular_buffer.SignalPoints();
}

uint32_t vuprs::HybridBeamformer::FIRLength() const
{
    return this->fir.FIRLength();
}

double vuprs::HybridBeamformer::InferenceFrameTimeMs() const
{
    return this->fault_detector.FrameTimeMs();
}

uint32_t vuprs::HybridBeamformer::InferenceFrameSamples(double fs) const
{
    return this->fault_detector.FrameLengthSamples(fs);
}

void vuprs::HybridBeamformer::ScanSwitch(bool enable)
{
    this->scan_enable = enable;
    this->scan_cv.notify_all();
}

bool vuprs::HybridBeamformer::ScanSwitch() const
{
    return this->scan_enable;
}

bool vuprs::HybridBeamformer::InitHybridBeamformer(const std::string &fpga_config_json,
                                                   const std::string &bf_array_config_json,
                                                   const std::string &fir_config_json,
                                                   const std::string &log_dir)
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
        RUNTIME_CHECK(false, "hybrid_bf", " Error occurred in initialization.");
    }
    this->hybrid_logger = vuprs::LogManager::getLogger("hybrid",
                                                       "log.txt",
                                                       log_dir);
    HYBRID_LOG(V_DEBUG) << "Successfully init logger: hybrid";
    this->config_done = operate_status;
    return operate_status;
}

bool vuprs::HybridBeamformer::InitInference(const std::string &model_config_json,
                                            const std::string &inference_log_dir)
{
#if ENABLE_INFERENCE
    try
    {
        this->inference_ready = this->fault_detector.InitDetector(model_config_json, inference_log_dir);
    }
    catch (const std::exception &e)
    {
        std::cout << "[hybrid] inference init failed: " << e.what() << std::endl;
        this->inference_ready = false;
    }
    return this->inference_ready;
#else
    this->inference_ready = false;
    return true;
#endif
}

bool vuprs::HybridBeamformer::CheckScanningConfigValid(const ScanningConfig &config, std::string *info) const
{
    bool retval = true;
#define _CHECK_AND_RETURN(STR)        \
    if (!retval)                      \
    {                                 \
        if (info != nullptr)          \
            *info = std::string(STR); \
        return false;                 \
    }
    retval &= config.points_in_hemisphere > 0 && config.points_in_hemisphere <= MAX_SCANNING_POINTS_IN_HALF;
    _CHECK_AND_RETURN("points_in_hemisphere must be in [1, MAX_SCANNING_POINTS_IN_HALF]")
    retval &= std::isfinite(config.alt_min) &&
              config.alt_min > MIN_SCANNING_ALTITUDE_DEG && config.alt_min <= 90.0;
    _CHECK_AND_RETURN("alt_min must be finite, > 10.0 deg and <= 90.0 deg")
    return retval;
}

bool vuprs::HybridBeamformer::ScanOptions(const ScanningConfig &config, double wave_velocity)
{
    PARAM_CHECK(this->CheckScanningConfigValid(config, nullptr), "hybrid", "Invalid scan options.");
    PARAM_CHECK(std::isfinite(wave_velocity) && wave_velocity > 0.0, "hybrid", "Invalid wave velocity.");
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(this->mut_scan_opt); /* LOCK */
        if (config.points_in_hemisphere != this->scan_points_in_hemisphere ||
            config.alt_min != this->scan_alt_min ||
            wave_velocity != this->scan_wave_velocity)
        {
            this->scan_points_in_hemisphere = config.points_in_hemisphere;
            this->scan_alt_min = config.alt_min;
            vuprs::FibonacciGrid(this->scan_points_in_hemisphere,
                                 this->scan_alt_min,
                                 &this->scan_alt,
                                 &this->scan_az);
            this->scan_wave_velocity = wave_velocity;
            changed = true;
        }
    }
    if (changed)
    {
        ++this->scan_options_generation;
        this->scan_options_changed = true;
        this->scan_cv.notify_all();
    }
    return true;
}

bool vuprs::HybridBeamformer::CheckConfigValid(const vuprs::HybridBeamformerConfig &config, std::string *info) const
{
    bool retval = true;
#define _CHECK_AND_RETURN(STR)        \
    if (!retval)                      \
    {                                 \
        if (info != nullptr)          \
            *info = std::string(STR); \
        return false;                 \
    }
    /* FPGA hardware must be initialized first */
    retval &= this->controller.ConfigDone();
    _CHECK_AND_RETURN("config not complete")
    /* Sampling frequency must be within the range supported by the ADC controller */
    retval &= std::isfinite(config.fs) &&
              (config.fs >= MIN_SAMPLING_FREQUENCY && config.fs <= this->controller.dev__adc_controller.MaxSamplingFrequency()) &&
              (this->controller.dev__adc_controller.GetSCIValueForSamplingFrequency(config.fs) != 0xFFFFFFFF);
    _CHECK_AND_RETURN("fs out of range or not supported by the ADC (must be min_fs <= fs <= max_fs)")
    /* Bandpass frequency range must be positive and below the Nyquist frequency */
    retval &= std::isfinite(config.bf_freq__lower) &&
              (config.bf_freq__lower >= MIN_BEAMFORMER_FREQUENCY && config.bf_freq__lower < config.fs / 2.0);
    retval &= std::isfinite(config.bf_freq__upper) &&
              (config.bf_freq__upper >= MIN_BEAMFORMER_FREQUENCY && config.bf_freq__upper < config.fs / 2.0);
    retval &= (config.bf_freq__lower < config.bf_freq__upper);
    _CHECK_AND_RETURN("bf_freq out of range (must be min_frequency <= lower < upper < fs/2)")
    /* Pointing direction must be within the physical hemisphere */
    retval &= std::isfinite(config.bf_target__alt) && (config.bf_target__alt >= -90.0 && config.bf_target__alt <= 90.0);
    retval &= std::isfinite(config.bf_target__az) && (config.bf_target__az >= -180.0 && config.bf_target__az <= 180.0);
    retval &= std::isfinite(config.bf_wave_velocity) &&
              (config.bf_wave_velocity > 0 && config.bf_wave_velocity <= MAX_WAVE_VELOCITY);
    _CHECK_AND_RETURN("bf_target out of range (alt in [-90,90], az in [-180,180], 0 < wave_velocity <= max)")
    /* Covariance matrix fitting parameters */
    retval &= std::isfinite(config.bf_cov_snapshots_window_size) &&
              (config.bf_cov_snapshots_window_size >= MIN_COVARIANCE_SNAPSHOT_WINDOW);
    retval &= std::isfinite(config.bf_cov_freq_average_index) && (config.bf_cov_freq_average_index >= 0.0 && config.bf_cov_freq_average_index < 1.0);
    _CHECK_AND_RETURN("bf_cov out of range (window_size >= 1, freq_average_index in [0,1))")
    /* DMA buffers: non-zero size, aligned, and the total size must fit in DDR (use uint64 to avoid overflow) */
    retval &= (config.dma__buffer_count > 0 && config.dma__buffer_points > 0);
    uint64_t total_ddr_capacity = static_cast<uint64_t>(config.dma__buffer_count) *
                                  static_cast<uint64_t>(config.dma__buffer_points) * sizeof(uint32_t);
    retval &= (total_ddr_capacity <= static_cast<uint64_t>(this->controller.mem__ddr.MaxSizeBytes()));
    _CHECK_AND_RETURN("dma buffer size/count invalid or total exceeds DDR capacity")
    /* Data queues */
    retval &= (config.queue__circular_buffer_queue_size_max > 0);
    _CHECK_AND_RETURN("queue size must be > 0")
    return retval;
#undef _CHECK_AND_RETURN
}

bool vuprs::HybridBeamformer::ResetHardwareBeamformer()
{
    bool retval = true;
    /* FPGA reset */
    HYBRID_LOG(V_DEBUG) << "Reset hardware beamformer [start].";
    retval &= vuprs::FPGA_API__ADC__ResetADC(&this->controller);             /* Reset ADC controller */
    retval &= vuprs::FPGA_API__CBUF__ResetCircularBuffer(&this->controller); /* Reset Circular Buffer */
    retval &= vuprs::FPGA_API__FIR__ResetFIR(&this->controller);             /* Reset FIR Filter Bank */
    retval &= vuprs::FPGA_API__DMA__ResetDMA(&this->controller);             /* Reset AXI DMA */
    HYBRID_LOG(V_DEBUG) << "Reset hardware beamformer [done].";
    this->system_run = false;
    return retval;
}

bool vuprs::HybridBeamformer::StartBeamformerWithConfiguration(const vuprs::HybridBeamformerConfig &config)
{
    PARAM_CHECK(this->ConfigDone(), "hybrid_bf", " Config not complete.");
    Eigen::Matrix<Eigen::dcomplex, -1, -1> fir_expected_frequency_response; /* Expected frequency response of FIR filter bank */
    std::vector<std::vector<double>> fir_coefficients;                      /* Coefficient of FIR filter bank */
    std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> _dma_descriptors;   /* SG descriptors for AXI DMA */
    std::vector<int> predelay_count;
    std::vector<double> predelay_time;
    std::vector<std::string> channel_name;
    int descriptor_update_cycle_us;
    uint32_t fir_len;
    bool retval = true;
    HYBRID_LOG(V_DEBUG) << "Start hybrid beamformer with parameters:";
    HYBRID_LOG(V_DEBUG) << "Bandpass frequency range [" + std::to_string(config.bf_freq__lower) + ", " +
                               std::to_string(config.bf_freq__upper) + "]";
    HYBRID_LOG(V_DEBUG) << "Beamformer pointing position [Alt = " + std::to_string(config.bf_target__alt) + ", Az = " +
                               std::to_string(config.bf_target__az) + "]";
    HYBRID_LOG(V_DEBUG) << "DDR buffer size = " + std::to_string(config.dma__buffer_points * sizeof(uint32_t)) + " bytes.";
    HYBRID_LOG(V_DEBUG) << "DDR buffer count = " + std::to_string(config.dma__buffer_count);
    {
        const uint32_t length_width = this->controller.dev__axi_dma.TransferLengthWidth();
        const uint64_t buffer_bytes = static_cast<uint64_t>(config.dma__buffer_points) * sizeof(uint32_t);
        RUNTIME_CHECK(length_width == 0 || length_width >= 32 || buffer_bytes < (1ULL << length_width),
                      "hybrid_bf", "DMA buffer exceeds S2MM length register width.");
    }
    /* Max queue size */
    this->circular_buffer_queue_size_max = config.queue__circular_buffer_queue_size_max;
    /* Step 1: Generate descriptors */
    {
        std::lock_guard<std::mutex> lock(this->mut); /* LOCK */
        this->sg_descriptor_config.buffer_count = config.dma__buffer_count;
        this->sg_descriptor_config.buffer_size = config.dma__buffer_points * sizeof(uint32_t);
        this->sg_descriptor_config.ddr_fpga_base_addr = this->controller.mem__ddr.FPGAAddress();
        this->sg_descriptor_config.sg_bram_fpga_base_addr = this->controller.mem__sg_bram.FPGAAddress();
        this->sg_descriptor_config.is_cyclic_dma_mode = true;
        vuprs::CreateDMAScatterGatherDescriptorChain(&this->dma_descriptors, this->sg_descriptor_config);
        _dma_descriptors = this->dma_descriptors;
        descriptor_update_cycle_us = static_cast<int>(1000000 * static_cast<double>(config.dma__buffer_points) / config.fs);
    }
    /* Step 2: Initialize loop cycle */
    this->interrupt_wait_time_us = descriptor_update_cycle_us / 20;
    this->circular_buffer_wait_time_us = descriptor_update_cycle_us / 20;
    HYBRID_LOG(V_DEBUG) << "DMA interrupt cycle time = " + std::to_string(this->interrupt_wait_time_us) + " us";
    HYBRID_LOG(V_DEBUG) << "Circular buffer interrupt cycle time = " + std::to_string(this->circular_buffer_wait_time_us) + " us";
#if VUPRS_DEBUG
    printf("DMA descriptor update cycle: %d us\n", descriptor_update_cycle_us);
    printf("DMA interrupt wait time: %d us\n", this->interrupt_wait_time_us.load());
    printf("Circular buffer interrupt wait time: %d us\n", this->circular_buffer_wait_time_us.load());
#endif
    /* Step 3: Get sampling frequency register (SCI) */
    uint32_t SCI = this->controller.dev__adc_controller.GetSCIValueForSamplingFrequency(config.fs);
    if (SCI == 0xFFFFFFFF)
    {
        HYBRID_LOG(V_WARN) << "Invalid sampling frequency: " + std::to_string(config.fs);
        return false;
    }
    HYBRID_LOG(V_DEBUG) << "Register [SCI] of [adc controller] has value: " + vuprs::Number2HexString(SCI);
    /* Step 4: Reset and initialize algorithm obj */
    {
        std::lock_guard<std::mutex> lock(this->mut_alg); /* LOCK */
        /* - Hardware sampling frequency */
        this->hardware_fs = this->controller.dev__adc_controller.SCI2FS(SCI);
        HYBRID_LOG(V_DEBUG) << "Hardware fs = " + std::to_string(this->hardware_fs.load()) + " Hz.";
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
                                              this->hardware_fs.load(),
                                              true,
                                              &predelay_count,
                                              &predelay_time,
                                              &channel_name);
        /* - Set bandpass range */
        this->fir.SetFrequencyRange(config.bf_freq__lower, config.bf_freq__upper);
        this->bf->SetWorkFrequencyRange(config.bf_freq__lower, config.bf_freq__upper);
        this->bf_freq__lower = config.bf_freq__lower; /* used by scan to filter out-of-band frequency bins */
        this->bf_freq__upper = config.bf_freq__upper;
        /* - Get zero FIR coefficients */
        this->fir.GetZeroFIRBankCoefficient(&fir_coefficients, this->bf->ElementCount());
        fir_len = this->fir.FIRLength();
    }
#if ENABLE_INFERENCE
    /* Step 4.5: Check whether one DMA buffer can provide at least one inference frame */
    {
        std::lock_guard<std::mutex> lock(this->mut_npu); /* LOCK */
        if (this->inference_ready &&
            !this->fault_detector.ValidateInputSignalLength(config.dma__buffer_points, this->hardware_fs.load()))
        {
            HYBRID_LOG(V_WARN) << "DMA buffer (" << config.dma__buffer_points << " samples) is shorter than one inference frame at fs = "
                               << this->hardware_fs.load() << " Hz. Fault inference will never start.";
        }
    }
#endif
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
                                                             fir_len);
    /* - FPGA config step 5 - Start ADC */
    retval &= vuprs::FPGA_API__ADC__StartADC(&this->controller, config.fs);
    RUNTIME_CHECK(retval, "hybrid_bf", " Cannot start beam former with config");
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
                                              this->hardware_fs.load(),
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
    if (!this->StartBeamformerWithConfiguration(config))
        return false;
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

bool vuprs::HybridBeamformer::stop()
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
    {
        std::lock_guard<std::mutex> lock(this->mut_scan_snapshot); /* LOCK */
        this->scan_snapshot.reset();
    }
    {
        std::lock_guard<std::mutex> lock(this->mut_queue); /* LOCK */
        this->array_signal_queue.clear();
    }
    this->circular_buffer_irq = false;
    this->dma_descriptor_irq = false;
    this->scan_options_changed = false;
    this->scan_options_initialized = false;
    this->dma_current_desc = 0xFFFFFFFF;
    this->threads.clear();
    try
    {
        return this->ResetHardwareBeamformer();
    }
    catch (const std::exception &e)
    {
        std::cout << "Error in [HybridBeamformer::stop] " << e.what() << std::endl;
        return false;
    }
}

/* ------------------------------------------ Thread Control ----------------------------------------- */

void vuprs::HybridBeamformer::THREAD__ScanPowerCalculation()
{
    vuprs::ScanResult scan_result;
    std::shared_ptr<const vuprs::ScanCovarianceSnapshot> cov_snapshot;
    std::vector<double> alt, az, _scan_result;
    double wave_velocity = 0.0, power_range = 0.0, alt_min = 0.0;
    int points_in_hemisphere = 0;
    size_t points_count = 0;
    uint16_t normalized_power = 0;
    uint64_t last_generation = 0, last_options_generation = 0, options_generation = 0, snapshot_generation = 0;
    bool calculate_status = false;
    bool options_loaded = false;
    while (this->system_run)
    {
        {
            std::unique_lock<std::mutex> lock(this->mut_scan_wait); /* LOCK */
            this->scan_cv.wait(lock, [this, last_generation, last_options_generation, options_loaded]()
                               { return !this->system_run ||
                                        (this->scan_enable &&
                                         (!this->consumer_gate_ || this->consumer_gate_()) &&
                                         (!options_loaded ||
                                          this->scan_options_generation.load() != last_options_generation ||
                                          !this->scan_options_initialized ||
                                          this->scan_generation.load() != last_generation)); });
        }
        if (!this->system_run)
            break; /* Jump out */
        if (!this->scan_enable)
            continue;
        /* Sync options */
        options_generation = this->scan_options_generation.load();
        if (!options_loaded ||
            options_generation != last_options_generation ||
            !this->scan_options_initialized)
        {
            std::lock_guard<std::mutex> lock(this->mut_scan_opt); /* LOCK */
            alt = this->scan_alt;
            az = this->scan_az;
            wave_velocity = this->scan_wave_velocity;
            points_in_hemisphere = this->scan_points_in_hemisphere;
            alt_min = this->scan_alt_min;
            this->scan_options_initialized = true;
            options_loaded = true;
        }
        last_options_generation = options_generation;
        const bool regenerate = this->scan_options_changed.load();
        /* Use the covariance snapshot captured by the algorithm thread. */
        {
            std::lock_guard<std::mutex> lock(this->mut_scan_snapshot); /* LOCK */
            if (!this->scan_snapshot)
            {
                last_generation = this->scan_generation.load();
                continue;
            }
            cov_snapshot = this->scan_snapshot;
            snapshot_generation = this->scan_generation.load();
        }
        /* Calculate scan power (lock-free, uses the consistent snapshot) */
        try
        {
            calculate_status = this->bf->ScanForPositionPower(&_scan_result,
                                                              &scan_result.max_power_db,
                                                              &scan_result.min_power_db,
                                                              alt,
                                                              az,
                                                              wave_velocity,
                                                              regenerate,
                                                              true,
                                                              *cov_snapshot);
        }
        catch (const std::exception &e)
        {
            std::cout << "Error in [HybridBeamformer::THREAD__ScanPowerCalculation] " << e.what() << std::endl;
            calculate_status = false;
        }
        last_generation = snapshot_generation;
        if (!calculate_status)
            continue; /* If calculation failed, skip this loop */
        if (regenerate && this->scan_options_generation.load() == options_generation)
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
        scan_result.points_in_hemisphere = points_in_hemisphere;
        scan_result.alt_min = alt_min;
        /* Deliver to the registered consumer (none: drop the frame). */
        if (this->scan_callback_)
        {
            try
            {
                this->scan_callback_(std::move(scan_result));
            }
            catch (...)
            {
                /* Never let a consumer take down the scan thread. */
            }
        }
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
#if VUPRS_DEBUG
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
    std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> _ref_descriptors;
    vuprs::AlignedBufferDMA buffer;
    std::vector<double> result_d;
    Eigen::Matrix<double, -1, 1> inference_res;
    bool has_interrupt;
    int last_read_index = -1;
    double _hardware_fs;
#if VUPRS_DEBUG
    int debug_file_group = 0;
#endif
    {
        std::lock_guard<std::mutex> lock(this->mut); /* LOCK */
        _ref_descriptors = this->dma_descriptors;
    }
    _hardware_fs = this->hardware_fs.load();
    auto process_descriptor = [&](const vuprs::AXI_DMA_ScatterGatherDescriptor &desc)
    {
        /* Skip the whole frame (DDR read + inference) when no consumer is attached. */
        if (this->consumer_gate_ && !this->consumer_gate_())
            return;
        try
        {
            /* Read completed buffer from DDR */
            if (!vuprs::FPGA_API__DDR__ReadDDR(&this->controller,
                                               &buffer,
                                               desc.BUFFER_ADDRESS,
                                               desc.ALIGNMENT_2_BUFFER_SIZE))
                return;
            HYBRID_LOG(V_DEBUG) << "Read bf result from buffer, with FPGA address ["
                                << vuprs::Number2HexString(desc.BUFFER_ADDRESS) << "], size ["
                                << desc.ALIGNMENT_2_BUFFER_SIZE << "]";
            vuprs::BeamformerResultMeta result_meta;
            /* Convert buffer to vector */
            result_meta.signal = buffer.to_vector<uint32_t>();
#if ENABLE_INFERENCE || VUPRS_DEBUG
            /* Convert to double */
            vuprs::Q16__UINT32_TO_DOUBLE(result_meta.signal, &result_d);
#endif
#if ENABLE_INFERENCE
            /* Inference: only if the model was initialized successfully. */
            if (this->inference_ready)
            {
                std::lock_guard<std::mutex> lock(this->mut_npu); /* LOCK */
                this->fault_detector.InputSignal(result_d, _hardware_fs);
                if (this->fault_detector.CheckReady())
                {
                    this->fault_detector.RunInference();
                    this->fault_detector.GetResult(&inference_res,
                                                   &result_meta.inference_result_identity,
                                                   false);
                    /* Convert to uint32_t Q31 inference result */
                    vuprs::Q31__DOUBLE_TO_UINT32(inference_res,
                                                 &result_meta.inference_result,
                                                 inference_res.array().abs().maxCoeff());
                    result_meta.inference_valid = true;
                }
            }
#endif
#if VUPRS_DEBUG
            buffer.to_file(std::string(DEBUG_FILES_ROOT_DIR) + "/" +
                           std::string(DEBUG_FILES_DIR) + "-" + std::to_string(debug_file_group) + "/" +
                           std::string(FIR_RESULT_BIN_DEBUG_FILENAME));
            vuprs::SaveToCSV(result_d, std::string(DEBUG_FILES_ROOT_DIR) + "/" +
                                           std::string(DEBUG_FILES_DIR) + "-" + std::to_string(debug_file_group) + "/" +
                                           std::string(FIR_RESULT_DEBUG_FILENAME));
            debug_file_group++;
            if (debug_file_group >= DEBUG_DATA_GROUP_COUNT)
                debug_file_group = 0;
#endif
            /* Deliver to the registered consumer (none: drop the frame). */
            if (this->result_callback_)
            {
                this->result_callback_(std::move(result_meta));
            }
        }
        catch (const std::exception &e)
        {
            std::cout << "Error in [HybridBeamformer::THREAD__ReadResult] " << e.what() << std::endl;
        }
    };
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
        has_interrupt = this->dma_descriptor_irq.load();
        this->dma_descriptor_irq = false; /* Clear interrupt */
        if (!has_interrupt)
            continue;
        const int descriptor_count = static_cast<int>(_ref_descriptors.size());
        if (descriptor_count <= 0)
            continue;
        const uint32_t current_addr = this->dma_current_desc.load();
        int current_index = -1;
        for (int i = 0; i < descriptor_count; i++)
        {
            if (_ref_descriptors[i].ALIGNMENT_0_CURRENT_ADDR == current_addr)
            {
                current_index = i;
                break;
            }
        }
        if (current_index < 0)
        {
            std::cout << "Warning in [HybridBeamformer::THREAD__ReadResult] Cannot match current descriptor address to any in the reference list." << std::endl;
            continue;
        }
        const int previous_index = (current_index + descriptor_count - 1) % descriptor_count;
        int pending = (last_read_index < 0) ? 1 : (previous_index - last_read_index + descriptor_count) % descriptor_count;
        int start_index = (last_read_index < 0) ? previous_index : (last_read_index + 1) % descriptor_count;
        for (int i = 0; i < pending; i++)
        {
            process_descriptor(_ref_descriptors[(start_index + i) % descriptor_count]);
        }
        last_read_index = previous_index;
    }
}

void vuprs::HybridBeamformer::THREAD__ReadCircularBuffer()
{
    uint32_t r_val = 0;
    while (this->system_run)
    {
        /* Check refreshed */
        r_val = 0;
        try
        {
            this->controller.dev__circular_buffer.ReadSingleRegisterBIT(vuprs::Circular_Buffer__Registers::CBUF_RS, 1, &r_val);
#if VUPRS_DEBUG
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
                vuprs::SignalData multi_channel_signal; /* signal data (from circular buffer) */
                if (vuprs::FPGA_API__CBUF__ReadCircularBuffer(&this->controller, &multi_channel_signal))
                {
                    {
                        std::lock_guard<std::mutex> lock(this->mut_queue); /* LOCK */
                        this->array_signal_queue.push_back(std::move(multi_channel_signal));
                        if (this->array_signal_queue.size() > this->circular_buffer_queue_size_max)
                        {
                            this->array_signal_queue.pop_front(); /* Pop the oldest data to avoid overflow */
                        }
                    }
                    this->circular_buffer_irq = true;
                    algorithm_cv.notify_all();
                }
                else
                {
                    RUNTIME_CHECK(false, "hybrid_bf", " Cannot read circular buffer.");
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
    bool has_interrupt, operation_status;
    vuprs::SignalData signal;
    Eigen::Matrix<Eigen::dcomplex, -1, -1> fir_expected_frequency_response; /* Expected frequency response of FIR filter bank */
    std::vector<std::vector<double>> fir_coefficients;                      /* Coefficient of FIR filter bank, [channel][point] */
    std::vector<std::string> channel_name;                                  /* Channel name */
#if VUPRS_DEBUG
    int debug_file_group = 0;
#endif
    while (this->system_run)
    {
        /* Sleep here to wait interrupt */
        {
            std::unique_lock<std::mutex> lock(this->mut_queue); /* LOCK */
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
        try
        {
            {
                std::lock_guard<std::mutex> lock(this->mut_queue); /* LOCK */
                if (this->array_signal_queue.empty())
                    continue;
                /* Read signal data from queue (moves the channel buffers) */
                signal = std::move(this->array_signal_queue.front());
                this->array_signal_queue.pop_front();
            }
#if VUPRS_DEBUG
            signal.ToCSV(std::string(DEBUG_FILES_ROOT_DIR) + "/" +
                         std::string(DEBUG_FILES_DIR) + "-" + std::to_string(debug_file_group) + "/" +
                         std::string(CIRCULAR_BUFFER_DEBUG_FILENAME));
#endif
            std::lock_guard<std::mutex> lock(this->mut_alg); /* LOCK */
            /* Step 1: Push data to Beam forming algorithm (channel buffers are moved in) */
            this->bf->InputSignal(std::move(signal));
            /* Step 2: Update covariance matrix */
            this->bf->UpdateCovarianceMatrix();
            /* Publish a covariance snapshot for the scan thread, so it never blocks on mut_alg.
             * Skip it when nobody is consuming scan results (e.g. client disconnected). */
            if (this->scan_enable &&
                (!this->consumer_gate_ || this->consumer_gate_()))
            {
                auto snapshot = std::make_shared<vuprs::ScanCovarianceSnapshot>();
                std::lock_guard<std::mutex> snapshot_lock(this->mut_scan_snapshot); /* LOCK */
                if (this->bf->SnapshotScanCovariance(snapshot.get(),
                                                     this->bf_freq__lower,
                                                     this->bf_freq__upper))
                {
                    this->scan_snapshot = std::move(snapshot);
                    ++this->scan_generation;
                }
                else
                {
                    this->scan_snapshot.reset();
                }
                this->scan_cv.notify_all();
            }
            /* - Check calculate enabled */
            RUNTIME_CHECK(this->bf->CalculateEnable(), "hybrid_bf", " Beam forming algorithm cannot calculate.");
            /* Step 3: Calculate beamforming */
            this->bf->CalculateBeamforming();
            /* Step 4: Get FIR filter bank expected frequency response */
            this->bf->GetFIRExpectedFrequencyResponse(&fir_expected_frequency_response,
                                                      &channel_name,
                                                      true);
            /* Step 5: Convert frequency response to FIR coefficients */
            this->fir.SolveCoeffUseExpectedFrequencyResponse(fir_expected_frequency_response,
                                                             channel_name,
                                                             this->hardware_fs.load());
            this->fir.GetFIRBankCoefficient(&fir_coefficients);
        }
        catch (const std::exception &e)
        {
            std::cout << "Error in [HybridBeamformer::THREAD__AlgorithmCalculation] " << e.what() << std::endl;
            continue;
        }
#if VUPRS_DEBUG
        printf("[debug] max FIR coefficient = %.8f\n", this->fir.MaxAbsoluteFIRCoefficient());
        debug_file_group++;
        if (debug_file_group >= DEBUG_DATA_GROUP_COUNT)
            debug_file_group = 0;
#endif
        /* Issue coefficients to FIR */
        operation_status = true;
        try
        {
            operation_status &= vuprs::FPGA_API__FIR__SetCoefficients(&this->controller,
                                                                      &fir_coefficients,
                                                                      this->fir.MaxAbsoluteFIRCoefficient());
            RUNTIME_CHECK(operation_status, "hybrid_bf", "FPGA operation failed.");
        }
        catch (const std::exception &e)
        {
            std::cout << "Error in [HybridBeamformer::THREAD__AlgorithmCalculation] " << e.what() << std::endl;
        }
    }
}
