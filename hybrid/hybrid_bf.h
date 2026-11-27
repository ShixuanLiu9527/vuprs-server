#ifndef HYBRID_BEAMFORMGING__H
#define HYBRID_BEAMFORMGING__H

#include <mutex>
#include <memory>
#include <deque>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <string>
#include "algo/bf/beam_former.h"
#include "algo/bf/fir.h"
#include "hybrid/hybrid_bf_config.h"
#include "fpga/fpga_api.h"
#include "algo/detector/fault_detector.h"
#include "logger/log_manager.h"

namespace vuprs
{
    struct BeamformerResultMeta
    {
        std::vector<uint32_t> signal; /* raw signal from DDR (Q16) */
    };

    struct InferenceResultMeta
    {
        std::vector<uint32_t> tensor; /* model output tensor (Q31) */
    };

    /**
     * @brief ARM FPGA Collaboration Beamformer.
     *
     * @note This class is designed for ARM FPGA collaboration beam former,
     * @note which includes hardware (FPGA) and algorithm (CPU) part.
     * @note The hardware part is responsible for data acquisition and pre-processing,
     * @note while the algorithm part is responsible for beam forming calculation.
     * @note The two parts are connected by AXI DMA and circular buffer,
     * @note and the data transfer is controlled by interrupts. The beam forming algorithm can
     * @note be customized by user, and the FPGA configuration can be customized by user through JSON file.
     *
     * @note Usage:
     * @note --- ---
     * @note Step 1: Create HybridBeamformer obj.
     * @note Step 2: Call InitCollaborationBeamformer() to initialize FPGA controller and algorithm.
     * @note Step 3: Call BindBeamformer() to bind beam forming algorithm (optional, if not called, default algorithm DCRCB will be used).
     * @note Step 4: Call RUN() to start beam former.
     * @note Step 5: Register SetResultCallback() / SetScanCallback() before RUN().
     * @note Step 6: Call STOP() to stop & reset beam former.
     */
    class HybridBeamformer
    {
    private:
        std::shared_ptr<spdlog::logger> hybrid_logger;

        bool config_done;
        std::vector<std::thread> threads;                                    /* Beam former threads */
        vuprs::FPGAController controller;                                    /* FPGA controller */
        std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> dma_descriptors; /* SG descriptors for AXI DMA */
        vuprs::AXI_DMA_SGDescriptor_Config sg_descriptor_config;             /* SG descriptor config */

        /**
         * @brief Start beam former (hardware & algorithm).
         */
        bool StartBeamformerWithConfiguration(const vuprs::HybridBeamformerConfig &config);

        /**
         * @brief Reset FPGA.
         *
         * @throw std::runtime_error
         */
        bool ResetHardwareBeamformer();

        /* Thread parameters */
        std::mutex mut; /* Global mutex lock */

        /* NPU / inference */
        std::mutex mut_npu;
        FaultDetector fault_detector;
        std::atomic<bool> inference_ready{false}; /* true only after successful model init */
        std::mutex mut_inference_wait;
        std::condition_variable inference_cv;
        bool inference_pending = false; /* protected by mut_inference_wait */

        /* Scan */
        std::mutex mut_scan_opt;                                            /* Scan mutex lock */
        std::vector<double> scan_alt, scan_az;                              /* controlled by mut_scan_opt */
        int scan_points_in_hemisphere;                                      /* controlled by mut_scan_opt */
        double scan_wave_velocity, scan_alt_min;                            /* controlled by mut_scan_opt */
        std::atomic<uint64_t> scan_generation{0};                           /* incremented when a new covariance snapshot is ready (at most SCAN_MAX_RATE_HZ per second) */
        std::atomic<uint64_t> scan_options_generation{0};                   /* incremented when points/alt_min/wave velocity change */
        std::mutex mut_scan_snapshot;                                       /* protects scan_snapshot */
        std::shared_ptr<const vuprs::ScanCovarianceSnapshot> scan_snapshot; /* cached covariance snapshot for scan */
        std::mutex mut_scan_wait;                                           /* Scan wait mutex [controlled by scan_cv] */
        std::condition_variable scan_cv;                                    /* Scan condition var, controlled by mut_scan_wait */

        /* Circular buffer interrupt */
        std::mutex mut_queue;                                  /* Array signal queue mutex */
        std::condition_variable algorithm_cv;                  /* Algorithm interrupt condition var, [controlled by mut_queue] */
        std::deque<vuprs::SignalData> array_signal_queue;      /* Array signal queue, [controlled by mut_queue] */
        std::mutex mut_alg;                                    /* Algorithm mutex lock (bf/fir/hardware_fs) */
        vuprs::FIRCalculator fir;                              /* FIR algorithm, [controlled by mut_alg] (can be only used in thread_algorithm) */
        std::unique_ptr<vuprs::WidebandBeamformerTemplate> bf; /* Beam forming algorithm, [controlled by mut_alg] (can be only used in thread_algorithm) */
        std::string bf_array_config_json_;                     /* Array config path for binding a new bf */
        std::shared_ptr<vuprs::ThreadPool> shared_thread_pool; /* One pool for all bf/scan/fir/array parallel work, so workers never exceed the core count */
        std::atomic<double> hardware_fs{0.0};                  /* Hardware sampling frequency, calculate by SCI register */
        double bf_freq__lower;                                 /* Bandpass lower boundary (Hz), set at start, read-only afterwards */
        double bf_freq__upper;                                 /* Bandpass upper boundary (Hz), set at start, read-only afterwards */
        /* Last successfully applied target [mut_alg]. */
        double target_alt_ = 0.0;
        double target_az_ = 0.0;
        double target_wave_velocity_ = 0.0;
        bool target_valid_ = false;

        /* DMA Interrupt */
        std::atomic<uint32_t> dma_current_desc{0xFFFFFFFF}; /* current descriptor address, initialized to an invalid value */
        std::mutex mut_dma;                                 /* DMA Interrupt mutex lock */
        std::condition_variable dma_interrupt_cv;           /* DMA Interrupt condition var, [controlled by mut_dma] */

        /* Atomics */
        std::atomic<bool> system_run{false};                  /* system run enable */
        std::atomic<bool> circular_buffer_irq{false};         /* Circular buffer interrupt flag */
        std::atomic<bool> dma_descriptor_irq{false};          /* DMA Interrupt flag */
        std::atomic<int> interrupt_wait_time_us{0};           /* = descriptor_update_cycle_us / 10 */
        std::atomic<int> circular_buffer_wait_time_us{0};     /* = descriptor_update_cycle_us / 5 */
        std::atomic<uint32_t> circular_buffer_queue_size_max; /* MAX size of circular buffer queue */
        std::atomic<bool> scan_enable{false};                 /* Scan enable flag */
        std::atomic<bool> scan_options_changed{false};        /* Scan options changed flag */
        std::atomic<bool> scan_options_initialized{false};    /* Scan options initialized flag */

        /* Optional consumers; set before run() and left unchanged while running. */
        std::function<void(BeamformerResultMeta &&)> result_callback_;
        std::function<void(ScanResult &&)> scan_callback_;
        std::function<void(InferenceResultMeta &&)> inference_callback_;
        std::function<bool()> consumer_gate_; /* optional: skip a frame when false */

        /* Threads */

        /**
         * @brief Listen to interrupt.
         *
         * @note If interrupt detected, dma_descriptor_irq <-- true.
         * @note Control mutex lock: mut_dma.
         */
        void thread_listen_bf();

        /**
         * @brief After interrupted, read data from FPGA DDR and deliver it to the result callback.
         *
         * @note STEP 1: Wait AXI DMA interrupt.
         * @note STEP 2: Read DDR to buffer.
         * @note STEP 3: Push data to the result callback.
         * @note Control mutex lock: mut_dma.
         */
        void thread_read_bf();

        /**
         * @brief Read circular buffer and push data to the array signal queue.
         *
         * @note If interrupt detected, circular_buffer_irq <-- true.
         * @note Control mutex lock: mut_queue.
         */
        void thread_read_cbuf();

        /**
         * @brief Pop the array signal queue, do beam forming, and issue FIR coefficients.
         */
        void thread_algorithm();

        /**
         * @brief Get the covariance snapshot and do scan power calculation.
         */
        void thread_scan();

        /**
         * @brief Run model inference when woken by the BF result thread.
         */
        void thread_inference();

    public:
        HybridBeamformer();
        ~HybridBeamformer();

        /* ------ Part 1: Initialization ------ */

        /**
         * @brief Initialize FPGA controller & Beamforming algorithm.
         *
         * @param fpga_config_json FPGA config JSON file.
         * @param bf_array_config_json Beam forming array config JSON file.
         * @param fir_config_json FIR filter config JSON file.
         * @param log_dir Log directory.
         */
        bool InitHybridBeamformer(const std::string &fpga_config_json,
                                  const std::string &bf_array_config_json,
                                  const std::string &fir_config_json,
                                  const std::string &log_dir);

        bool InitInference(const std::string &model_config_json,
                           const std::string &inference_log_dir);

        /**
         * @brief Bind beam forming algorithm.
         *
         * @param beamformer beam forming algorithm (must be created by user, and bind to this class).
         * @note Call only when stopped; the remembered array config is applied automatically.
         */
        bool BindBeamformer(std::unique_ptr<vuprs::WidebandBeamformerTemplate> beamformer = nullptr);

        /**
         * @brief Indicate config done.
         *
         * @retval true: config done;
         * @retval false: config not complete.
         */
        bool ConfigDone() const;

        /**
         * @brief Set optional result consumers.
         *
         * @note Call before run(); callbacks execute on DSP/scan/inference threads and
         * @note must be non-blocking.
         */
        void SetResultCallback(std::function<void(BeamformerResultMeta &&)> callback);
        void SetScanCallback(std::function<void(ScanResult &&)> callback);
        void SetInferenceCallback(std::function<void(InferenceResultMeta &&)> callback);

        /**
         * @brief Set an optional gate checked on the DSP/scan threads before doing work.
         *
         * @note Return false to skip a frame entirely (e.g. no network client).
         * @note Call before run(); must be non-blocking.
         */
        void SetConsumerGate(std::function<bool()> gate);

        bool InferenceReady() const { return this->inference_ready.load(); }
        double HardwareSamplingFrequency();

        /* ------ Capability/limits queries ------ */
        double MaxSamplingFrequency() const;
        uint32_t SignalPoints() const;
        uint32_t FIRLength() const;
        double InferenceFrameTimeMs() const;
        uint32_t InferenceFrameSamples(double fs) const;
        uint32_t InferenceTensorLength() const;

        /* ------ Part 1: Control (Run & Stop) ------ */

        /**
         * @brief Indicate beam former has started.
         *
         * @note Tread safety.
         *
         * @retval true: beam former is running;
         * @retval false: beam former is not running.
         */
        bool isRun() const;

        /**
         * @brief Start beam former with configuration.
         *
         * @note Tread safety.
         *
         * @param config HybridBeamformerConfig struct.
         *
         * @retval true: success.
         * @retval false: failed.
         */
        bool run(const HybridBeamformerConfig &config);

        /**
         * @brief Check config valid.
         */
        bool CheckConfigValid(const vuprs::HybridBeamformerConfig &config, std::string *info) const;

        /**
         * @brief Stop & reset beam former.
         *
         * @note Tread safety.
         */
        bool stop();

        /* ------ Part 2: Control (for algorithms) ------ */

        /* - Part 2.1: Pointing position control of beamforming - */

        /**
         * @brief Change target direct of the beam former.
         *
         * @note Tread safety.
         *
         * @note The function can be called when the beam former is running,
         * @note and the direction of beamformer will be changed in real time.
         *
         * @param alt altitude (unit: degree) of beam former pointing target.
         * @param az azimuth (unit: degree) of beam former pointing target.
         * @param wave_velocity wave velocity (m/s). e.g. 346.0 for speed of sound in air.
         */
        bool ReDirect(double alt, double az, double wave_velocity);

        /* - Part 2.2: Scan control - */

        /**
         * @brief Set scan enable.
         *
         * @note Tread safety.
         *
         * @param enable true: enable scan; false: disable scan.
         */
        void ScanSwitch(bool enable);

        /**
         * @brief Indicate scan enable.
         *
         * @note Tread safety.
         *
         * @retval true: scan enabled;
         * @retval false: scan disabled.
         */
        bool ScanSwitch() const;

        /**
         * @brief Set scan options.
         *
         * @note Tread safety.
         *
         * @param config scanning config.
         * @param wave_velocity wave velocity.
         */
        bool ScanOptions(const ScanningConfig &config, double wave_velocity);

        /**
         * @brief Check config valid.
         */
        bool CheckScanningConfigValid(const ScanningConfig &config, std::string *info) const;

        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };
}

#endif
