#include <chrono>
#include <cmath>
#include "config.h"
#include "hybrid/hybrid_bf.h"
#include "logger/check.h"
#include "misc/rate_limiter.h"

#define HYBRID_LOG(level) _LOG(this->hybrid_logger ? this->hybrid_logger : spdlog::default_logger(), level)

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
    if (beamformer == nullptr)
        return false;
    RUNTIME_CHECK(!this->system_run, "hybrid_bf", " Cannot bind beamformer while running.");
    /* New bf has no array config; reuse the path saved by Init. */
    if (!this->bf_array_config_json_.empty() && !beamformer->ConfigArrayFromJson(this->bf_array_config_json_))
        return false;
    if (this->shared_thread_pool)
    {
        beamformer->SetThreadPool(this->shared_thread_pool);
    }
    {
        std::lock_guard<std::mutex> lock(this->mut_alg); /* LOCK */
        this->bf = std::move(beamformer);
        this->target_valid_ = false; /* New bf has no target yet. */
    }
    return true;
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

void vuprs::HybridBeamformer::SetInferenceCallback(std::function<void(InferenceResultMeta &&)> callback)
{
    this->inference_callback_ = std::move(callback);
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

uint32_t vuprs::HybridBeamformer::InferenceTensorLength() const
{
    return this->inference_ready.load() ? this->fault_detector.OutputTensorLength() : 0;
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
    bool array_config_ok = false;
    try
    {
        operate_status &= this->controller.ConfigFPGAFromJson(fpga_config_json);
        array_config_ok = this->bf->ConfigArrayFromJson(bf_array_config_json);
        operate_status &= array_config_ok;
        operate_status &= this->fir.ConfigFIRFromJsonFile(fir_config_json);
    }
    catch (const std::exception &e)
    {
        RUNTIME_CHECK(false, "hybrid_bf", " Error occurred in initialization.");
    }
    if (array_config_ok)
    {
        this->bf_array_config_json_ = bf_array_config_json;
    }
    this->hybrid_logger = vuprs::LogManager::getLogger("hybrid",
                                                       "log.txt",
                                                       log_dir);
    HYBRID_LOG(V_DEBUG) << "Successfully init logger: hybrid";
    /* Share one thread pool across beam forming (covariance/DCRCB), scan power, array FFT and
     * FIR IFFT, so the number of pool workers never exceeds the core count. */
    this->shared_thread_pool = std::make_shared<vuprs::ThreadPool>(std::thread::hardware_concurrency());
    this->bf->SetThreadPool(this->shared_thread_pool);
    this->fir.SetThreadPool(this->shared_thread_pool);
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
        HYBRID_LOG(V_ERROR) << "[hybrid] inference init failed: " << e.what();
        this->inference_ready = false;
    }
    return this->inference_ready;
#else
    this->inference_ready = false;
    return true;
#endif
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

#define CHECK_AND_RETURN(VAL, STR)        \
    do                                    \
    {                                     \
        if (!VAL)                         \
        {                                 \
            if (info != nullptr)          \
                *info = std::string(STR); \
            return false;                 \
        }                                 \
    } while (0)

bool vuprs::HybridBeamformer::CheckScanningConfigValid(const ScanningConfig &config, std::string *info) const
{
    bool retval = true;
    retval &= config.points_in_hemisphere > 0 && config.points_in_hemisphere <= MAX_SCANNING_POINTS_IN_HALF;
    CHECK_AND_RETURN(retval, "points_in_hemisphere must be in [1, MAX_SCANNING_POINTS_IN_HALF]");
    retval &= std::isfinite(config.alt_min) &&
              config.alt_min > MIN_SCANNING_ALTITUDE_DEG && config.alt_min <= 90.0;
    CHECK_AND_RETURN(retval, "alt_min must be finite, > 10.0 deg and <= 90.0 deg");
    return retval;
}

bool vuprs::HybridBeamformer::CheckConfigValid(const vuprs::HybridBeamformerConfig &config, std::string *info) const
{
    bool retval = true;
    /* FPGA hardware must be initialized first */
    retval &= this->controller.ConfigDone();
    CHECK_AND_RETURN(retval, "config not complete");
    /* Sampling frequency must be within the range supported by the ADC controller */
    retval &= std::isfinite(config.fs) &&
              (config.fs >= MIN_SAMPLING_FREQUENCY && config.fs <= this->controller.dev__adc_controller.MaxSamplingFrequency()) &&
              (this->controller.dev__adc_controller.GetSCIValueForSamplingFrequency(config.fs) != 0xFFFFFFFF);
    CHECK_AND_RETURN(retval, "fs out of range or not supported by the ADC (must be min_fs <= fs <= max_fs)");
    /* Bandpass frequency range must be positive and below the Nyquist frequency */
    retval &= std::isfinite(config.bf_freq__lower) &&
              (config.bf_freq__lower >= MIN_BEAMFORMER_FREQUENCY && config.bf_freq__lower < config.fs / 2.0);
    retval &= std::isfinite(config.bf_freq__upper) &&
              (config.bf_freq__upper >= MIN_BEAMFORMER_FREQUENCY && config.bf_freq__upper < config.fs / 2.0);
    retval &= (config.bf_freq__lower < config.bf_freq__upper);
    CHECK_AND_RETURN(retval, "bf_freq out of range (must be min_frequency <= lower < upper < fs/2)");
    /* Pointing direction must be within the physical hemisphere */
    retval &= std::isfinite(config.bf_target__alt) && (config.bf_target__alt >= -90.0 && config.bf_target__alt <= 90.0);
    retval &= std::isfinite(config.bf_target__az) && (config.bf_target__az >= -180.0 && config.bf_target__az <= 180.0);
    retval &= std::isfinite(config.bf_wave_velocity) &&
              (config.bf_wave_velocity > 0 && config.bf_wave_velocity <= MAX_WAVE_VELOCITY);
    CHECK_AND_RETURN(retval, "bf_target out of range (alt in [-90,90], az in [-180,180], 0 < wave_velocity <= max)");
    /* Covariance matrix fitting parameters */
    retval &= std::isfinite(config.bf_cov_snapshots_window_size) &&
              (config.bf_cov_snapshots_window_size >= MIN_COVARIANCE_SNAPSHOT_WINDOW);
    retval &= std::isfinite(config.bf_cov_freq_average_index) && (config.bf_cov_freq_average_index >= 0.0 && config.bf_cov_freq_average_index < 1.0);
    CHECK_AND_RETURN(retval, "bf_cov out of range (window_size >= 1, freq_average_index in [0,1))");
    /* DMA buffers: non-zero size, aligned, and the total size must fit in DDR (use uint64 to avoid overflow) */
    retval &= (config.dma__buffer_count > 0 && config.dma__buffer_points > 0);
    uint64_t total_ddr_capacity = static_cast<uint64_t>(config.dma__buffer_count) *
                                  static_cast<uint64_t>(config.dma__buffer_points) * sizeof(uint32_t);
    retval &= (total_ddr_capacity <= static_cast<uint64_t>(this->controller.mem__ddr.MaxSizeBytes()));
    CHECK_AND_RETURN(retval, "dma buffer size/count invalid or total exceeds DDR capacity");
    /* Data queues */
    retval &= (config.queue__circular_buffer_queue_size_max > 0);
    CHECK_AND_RETURN(retval, "queue size must be > 0");
    return retval;
}

#undef CHECK_AND_RETURN

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
    PARAM_CHECK(this->ConfigDone() && this->bf != nullptr && this->bf->ConfigDone(),
                "hybrid_bf", " Config not complete.");
    Eigen::Matrix<Eigen::dcomplex, -1, -1> fir_expected_frequency_response; /* Expected frequency response of FIR filter bank */
    std::vector<std::vector<double>> fir_coefficients;                      /* Coefficient of FIR filter bank */
    std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> _dma_descriptors;   /* SG descriptors for AXI DMA */
    std::vector<int> predelay_count;
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
                                              nullptr);
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
    retval &= vuprs::FPGA_API__PDLY__SetPredelay(&this->controller, predelay_count);
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
    /* Save target only after a successful start for ReDirect() rollback. */
    {
        std::lock_guard<std::mutex> lock(this->mut_alg); /* LOCK */
        this->target_alt_ = config.bf_target__alt;
        this->target_az_ = config.bf_target__az;
        this->target_wave_velocity_ = config.bf_wave_velocity;
        this->target_valid_ = true;
    }
    return retval;
}

bool vuprs::HybridBeamformer::ReDirect(double alt, double az, double wave_velocity)
{
    std::vector<int> predelay_count, old_predelay_count;
    std::lock_guard<std::mutex> lock(this->mut_alg); /* LOCK */
    const double old_alt = this->target_alt_;
    const double old_az = this->target_az_;
    const double old_wave_velocity = this->target_wave_velocity_;
    const bool old_target_valid = this->target_valid_;
    bool redirected = false;
    try
    {
        /* Update software target and hardware PDLY together under mut_alg. */
        this->bf->SetTargetDirection(alt, az, wave_velocity);
        this->bf->UpdateAndGetElementPredelay(this->fir.FIRLength(),
                                              this->hardware_fs.load(),
                                              true,
                                              &predelay_count,
                                              nullptr);
        redirected = vuprs::FPGA_API__PDLY__SetPredelay(&this->controller, predelay_count);
    }
    catch (const std::exception &e)
    {
        HYBRID_LOG(V_ERROR) << "Error in [HybridBeamformer::ReDirect] " << e.what();
        redirected = false;
    }
    if (redirected)
    {
        this->target_alt_ = alt;
        this->target_az_ = az;
        this->target_wave_velocity_ = wave_velocity;
        this->target_valid_ = true;
        return true;
    }
    /* Roll back software target and hardware PDLY. */
    if (old_target_valid)
    {
        try
        {
            this->bf->SetTargetDirection(old_alt, old_az, old_wave_velocity);
            this->bf->UpdateAndGetElementPredelay(this->fir.FIRLength(),
                                                  this->hardware_fs.load(),
                                                  true,
                                                  &old_predelay_count,
                                                  nullptr);
            if (!vuprs::FPGA_API__PDLY__SetPredelay(&this->controller, old_predelay_count))
            {
                HYBRID_LOG(V_ERROR) << "Error in [HybridBeamformer::ReDirect] rollback PDLY write failed.";
            }
        }
        catch (const std::exception &e)
        {
            HYBRID_LOG(V_ERROR) << "Error in [HybridBeamformer::ReDirect] rollback failed: " << e.what();
        }
    }
    return false;
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
                               { this->thread_read_bf(); });
    this->threads.emplace_back([this]()
                               { this->thread_listen_bf(); });
    this->threads.emplace_back([this]()
                               { this->thread_algorithm(); });
    this->threads.emplace_back([this]()
                               { this->thread_scan(); });
    this->threads.emplace_back([this]()
                               { this->thread_inference(); });
    this->threads.emplace_back([this]()
                               { this->thread_read_cbuf(); });
    return true;
}

bool vuprs::HybridBeamformer::stop()
{
    this->system_run = false;
    /* Lock each wait mutex once to close the predicate/sleep race before notify. */
    {
        std::lock_guard<std::mutex> lock(this->mut_queue); /* LOCK */
    }
    this->algorithm_cv.notify_all();
    {
        std::lock_guard<std::mutex> lock(this->mut_dma); /* LOCK */
    }
    this->dma_interrupt_cv.notify_all();
    {
        std::lock_guard<std::mutex> lock(this->mut_scan_wait); /* LOCK */
    }
    this->scan_cv.notify_all();
    {
        std::lock_guard<std::mutex> lock(this->mut_inference_wait); /* LOCK */
    }
    this->inference_cv.notify_all();
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
    {
        std::lock_guard<std::mutex> lock(this->mut_inference_wait); /* LOCK */
        this->inference_pending = false;
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
        HYBRID_LOG(V_ERROR) << "Error in [HybridBeamformer::stop] " << e.what();
        return false;
    }
}

/* ------------------------------------------ Thread Control ----------------------------------------- */

void vuprs::HybridBeamformer::thread_scan()
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
            this->scan_cv.wait(
                lock,
                [this, last_generation, last_options_generation, options_loaded]()
                {
                    return !this->system_run ||
                           (this->scan_enable && (!this->consumer_gate_ || this->consumer_gate_()) &&
                            (!options_loaded || this->scan_options_generation.load() != last_options_generation ||
                             !this->scan_options_initialized || this->scan_generation.load() != last_generation));
                });
        }
        if (!this->system_run)
            break; /* Jump out */
        if (!this->scan_enable)
            continue;
        /* Sync options */
        options_generation = this->scan_options_generation.load();
        if (!options_loaded || options_generation != last_options_generation || !this->scan_options_initialized)
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
            HYBRID_LOG(V_ERROR) << "Error in [HybridBeamformer::thread_scan] " << e.what();
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

void vuprs::HybridBeamformer::thread_inference()
{
    Eigen::Matrix<double, -1, 1> inference_res;
    while (this->system_run)
    {
        {
            std::unique_lock<std::mutex> lock(this->mut_inference_wait); /* LOCK */
            this->inference_cv.wait(lock, [this]()
                                    { return !this->system_run || this->inference_pending; });
            if (!this->system_run)
                break;
            this->inference_pending = false;
        }
        try
        {
            {
                std::lock_guard<std::mutex> lock(this->mut_npu); /* LOCK */
                if (!this->inference_ready || !this->fault_detector.CheckReady())
                    continue;
                this->fault_detector.RunInference();
                this->fault_detector.GetResult(&inference_res, nullptr, false);
            }
            if (inference_res.size() <= 0)
                continue;
            vuprs::InferenceResultMeta result_meta;
            vuprs::Q31__DOUBLE_TO_UINT32(inference_res,
                                         &result_meta.tensor,
                                         inference_res.array().abs().maxCoeff());
            if (this->inference_callback_)
                this->inference_callback_(std::move(result_meta));
        }
        catch (const std::exception &e)
        {
            HYBRID_LOG(V_ERROR) << "Error in [HybridBeamformer::thread_inference] " << e.what();
        }
    }
}

void vuprs::HybridBeamformer::thread_listen_bf()
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
            HYBRID_LOG(V_ERROR) << "Error in [HybridBeamformer::thread_listen_bf] Error: " << e.what();
        }
        if (!this->system_run)
            break; /* Jump out */
        std::this_thread::sleep_for(std::chrono::microseconds(this->interrupt_wait_time_us));
    }
}

void vuprs::HybridBeamformer::thread_read_bf()
{
    std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> _ref_descriptors;
    vuprs::AlignedBufferDMA buffer;
    std::vector<double> result_d;
    bool has_interrupt;
#if ENABLE_INFERENCE
    vuprs::RateLimiter inference_limiter(std::chrono::microseconds(1000000 / INFERENCE_MAX_RATE_HZ));
#endif
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
        /* Skip the whole frame when no consumer is attached. */
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
            /* Feed BF data; wake inference when ready and rate-limited. */
            if (this->inference_ready)
            {
                try
                {
                    bool ready = false;
                    {
                        std::lock_guard<std::mutex> lock(this->mut_npu); /* LOCK */
                        this->fault_detector.InputSignal(result_d, _hardware_fs);
                        ready = this->fault_detector.CheckReady();
                    }
                    if (ready)
                    {
                        bool notify = false;
                        {
                            std::lock_guard<std::mutex> lock(this->mut_inference_wait); /* LOCK */
                            if (!this->inference_pending && inference_limiter.Allow())
                            {
                                this->inference_pending = true;
                                notify = true;
                            }
                        }
                        if (notify)
                            this->inference_cv.notify_one();
                    }
                }
                catch (const std::exception &e)
                {
                    HYBRID_LOG(V_WARN) << "Inference feed failed: " << e.what();
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
            HYBRID_LOG(V_ERROR) << "Error in [HybridBeamformer::thread_read_bf] " << e.what();
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
            HYBRID_LOG(V_WARN) << "Warning in [HybridBeamformer::thread_read_bf] Cannot match current descriptor address to any in the reference list.";
            continue;
        }
        const int previous_index = (current_index + descriptor_count - 1) % descriptor_count;
        int pending = (last_read_index < 0) ? 1 : (previous_index - last_read_index + descriptor_count) % descriptor_count;
        int start_index = (last_read_index < 0) ? previous_index : (last_read_index + 1) % descriptor_count;
        for (int i = 0; i < pending; i++)
            process_descriptor(_ref_descriptors[(start_index + i) % descriptor_count]);
        last_read_index = previous_index;
    }
}

void vuprs::HybridBeamformer::thread_read_cbuf()
{
    uint32_t r_val = 0;
    while (this->system_run)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(this->circular_buffer_wait_time_us));
        if (!this->system_run)
            break;
        /* - Check refreshed - */
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
            HYBRID_LOG(V_ERROR) << "Error in [HybridBeamformer::thread_read_cbuf] " << e.what();
        }
        if (r_val != 0x01)
            continue; /* back to the sleep at the loop start */
        /* - Read circular buffer - */
        try
        {
            vuprs::SignalData multi_channel_signal; /* signal data (from circular buffer) */
            if (vuprs::FPGA_API__CBUF__ReadCircularBuffer(&this->controller, &multi_channel_signal))
            {
                {
                    std::lock_guard<std::mutex> lock(this->mut_queue); /* LOCK */
                    this->array_signal_queue.push_back(std::move(multi_channel_signal));
                    /* - Pop the oldest data to avoid overflow - */
                    if (this->array_signal_queue.size() > this->circular_buffer_queue_size_max)
                        this->array_signal_queue.pop_front();
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
            HYBRID_LOG(V_ERROR) << "Error in [HybridBeamformer::thread_read_cbuf]" << e.what();
        }
    }
}

void vuprs::HybridBeamformer::thread_algorithm()
{
    bool has_interrupt, operation_status;
    vuprs::SignalData signal;
    Eigen::Matrix<Eigen::dcomplex, -1, -1> fir_expected_frequency_response; /* Expected frequency response of FIR filter bank */
    std::vector<std::vector<double>> fir_coefficients;                      /* Coefficient of FIR filter bank, [channel][point] */
    /* Scan snapshot throttle: first capture happens immediately. */
    vuprs::RateLimiter scan_snapshot_limiter(std::chrono::microseconds(1000000 / SCAN_MAX_RATE_HZ));
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
            /* - Publish a covariance snapshot for the scan thread. - */
            /* - Calculation (driven by scan_generation) is capped at the same rate. - */
            if (this->scan_enable && (!this->consumer_gate_ || this->consumer_gate_()))
            {
                /* - Publish a covariance snapshot [SCAN_MAX_RATE_HZ] times per sec - */
                if (scan_snapshot_limiter.Allow())
                {
                    auto snapshot = std::make_shared<vuprs::ScanCovarianceSnapshot>();
                    std::lock_guard<std::mutex> snapshot_lock(this->mut_scan_snapshot); /* LOCK */
                    if (this->bf->SnapshotScanCovariance(snapshot.get(), this->bf_freq__lower, this->bf_freq__upper))
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
            }
            else
            {
                scan_snapshot_limiter.Reset(); /* Scan off: next enable captures immediately. */
            }
            /* - Check calculate enabled */
            RUNTIME_CHECK(this->bf->CalculateEnable(), "hybrid_bf", " Beam forming algorithm cannot calculate.");
            /* Step 3: Calculate beamforming */
            this->bf->CalculateBeamforming();
            /* Step 4: Get FIR filter bank expected frequency response */
            this->bf->GetFIRExpectedFrequencyResponse(&fir_expected_frequency_response, true);
            /* Step 5: Convert frequency response to FIR coefficients */
            this->fir.SolveCoeffUseExpectedFrequencyResponse(fir_expected_frequency_response, this->hardware_fs.load());
            this->fir.GetFIRBankCoefficient(&fir_coefficients);
        }
        catch (const std::exception &e)
        {
            HYBRID_LOG(V_ERROR) << "Error in [HybridBeamformer::thread_algorithm] " << e.what();
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
            HYBRID_LOG(V_ERROR) << "Error in [HybridBeamformer::thread_algorithm] " << e.what();
        }
    }
}
