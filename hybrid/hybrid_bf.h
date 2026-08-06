#ifndef HYBRID_BEAMFORMGING__H
#define HYBRID_BEAMFORMGING__H

#include <mutex>
#include <queue>
#include <deque>
#include <atomic>
#include <condition_variable>
#include "algorithm/bf/beam_former.h"
#include "algorithm/bf/fir.h"
#include "hybrid/hybrid_bf_config.h"
#include "fpga/fpga_api.h"
#include "fault_detect/fault_detector.h"
#include "logger/log_manager.h"

namespace vuprs
{
    struct BeamformerResultMeta
    {
        std::vector<uint32_t> signal;           /* raw signal from DDR (Q16) */
        std::vector<uint32_t> inference_result; /* inference result from NPU (Q31) */
        int inference_result_identity;          /* inference result class identity */
        bool inference_valid;
        BeamformerResultMeta() : inference_valid(false), inference_result_identity(-1) {}
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
     * @note Step 5: Call ReadResultFromQueue() to read result from queue.
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

        /* NPU */
        std::mutex mut_npu;
        FaultDetector fault_detector;

        /* Scan */
        std::mutex mut_scan_opt;                         /* Scan mutex lock */
        std::vector<double> scan_alt, scan_az;           /* controlled by mut_scan_opt */
        int scan_points_in_hemisphere;                   /* controlled by mut_scan_opt */
        double scan_wave_velocity, scan_alt_min;         /* controlled by mut_scan_opt */
        std::atomic<bool> new_scan_points_input{false};  /* scan points changed flag */
        std::mutex mut_scan_result;                      /* Scan result mutex lock */
        std::condition_variable scan_cv;                 /* Scan condition var, controlled by mut_scan */
        std::deque<vuprs::ScanResult> scan_result_queue; /* Scan result, controlled by mut_scan_result */

        /* Circular buffer interrupt */
        std::mutex mut_alg;                                      /* Algorithm mutex lock */
        std::condition_variable algorithm_cv;                    /* Algorithm interrupt condition var, [controlled by mut_alg] */
        std::deque<vuprs::SignalData> array_signal_queue;        /* Array signal queue, [controlled by mut_alg] */
        std::atomic<bool> new_array_signal_input{false};         /* new array signal input flag */
        std::mutex mut_output_arraySignal;                       /* Output array signal mutex lock */
        std::deque<vuprs::SignalData> output_array_signal_queue; /* Output array signal queue, controlled by mut_output_arraySignal */
        vuprs::FIRCalculator fir;                                /* FIR algorithm, [controlled by mut_alg] (can be only used in THREAD__AlgorithmCalculation) */
        std::unique_ptr<vuprs::WidebandBeamformerTemplate> bf;   /* Beam forming algorithm, [controlled by mut_alg] (can be only used in THREAD__AlgorithmCalculation) */
        double hardware_fs;                                      /* Hardware sampling frequency, calculate by SCI register, [controlled by mut_alg] */

        /* DMA Interrupt */
        std::atomic<uint32_t> dma_current_desc{0xFFFFFFFF}; /* current descriptor address, initialized to an invalid value */
        std::mutex mut_dma;                                 /* DMA Interrupt mutex lock */
        std::condition_variable dma_interrupt_cv;           /* DMA Interrupt condition var, [controlled by mut_dma] */
        std::atomic<bool> new_result_data_input{false};     /* assign to outside */
        std::deque<BeamformerResultMeta> result_queue;      /* Result queue, [controlled by mut_dma] */

        /* Atomics */
        std::atomic<bool> system_run{false};                  /* system run enable */
        std::atomic<bool> circular_buffer_irq{false};         /* Circular buffer interrupt flag */
        std::atomic<bool> dma_descriptor_irq{false};          /* DMA Interrupt flag */
        std::atomic<int> interrupt_wait_time_us{0};           /* = descriptor_update_cycle_us / 10 */
        std::atomic<int> circular_buffer_wait_time_us{0};     /* = descriptor_update_cycle_us / 5 */
        std::atomic<uint32_t> circular_buffer_queue_size_max; /* MAX size of circular buffer queue */
        std::atomic<uint32_t> result_queue_size_max;          /* MAX size of result queue */
        std::atomic<bool> scan_enable{false};                 /* Scan enable flag */
        std::atomic<bool> scan_options_changed{false};        /* Scan options changed flag */
        std::atomic<bool> scan_options_initialized{false};    /* Scan options initialized flag */

        /* Threads */

        /**
         * @brief Listen to interrupt.
         *
         * @note If interrupt detected, dma_descriptor_irq <-- true.
         * @note Control mutex lock: mut_dma.
         */
        void THREAD__ListenDMAInterrupt();

        /**
         * @brief After interrupted, read data from FPGA DDR to queue.
         *
         * @note STEP 1: Wait AXI DMA interrupt.
         * @note STEP 2: Read DDR to buffer.
         * @note STEP 3: Push data to beamformingResultQueue
         * @note Control mutex lock: mut_dma.
         */
        void THREAD__ReadResult();

        /**
         * @brief Read circular buffer and push data to queue.
         *
         * @note If interrupt detected, circular_buffer_irq <-- true.
         * @note Control mutex lock: mut_alg.
         */
        void THREAD__ReadCircularBuffer();

        /**
         * @brief Get data from queue and do beam forming calculate and push to queue.
         */
        void THREAD__AlgorithmCalculation();

        /**
         * @brief Get data from output array signal queue and do scan power calculation.
         */
        void THREAD__ScanPowerCalculation();

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
         */
        void BindBeamformer(std::unique_ptr<vuprs::WidebandBeamformerTemplate> beamformer = nullptr);

        /**
         * @brief Indicate config done.
         *
         * @retval true: config done;
         * @retval false: config not complete.
         */
        bool ConfigDone() const;

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
         * @brief Stop & reset beam former.
         *
         * @note Tread safety.
         */
        void stop();

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
         * @param points_in_hemisphere scanning points in half of the scanning area (altitude: 0-90 degree, azimuth: -180-180 degree).
         * @param alt_min minimum altitude (unit: degree) of scanning area.
         * @param wave_velocity wave velocity (m/s). e.g. 346.0 for speed of sound in air.
         */
        void ScanOptions(int points_in_hemisphere, double alt_min, double wave_velocity);

        /* ------ Part 3: Data Input/Output ------ */

        /* - Part 3.1: Beamforming result data Input/Output - */

        /**
         * @brief Indicate new result data input.
         *
         * @note Tread safety.
         *
         * @retval true: new result data input;
         * @retval false: no new result data input.
         */
        bool HasResult() const;

        /**
         * @brief Read result from result queue.
         *
         * @note Tread safety.
         *
         * @param meta beamformer result.
         *
         * @retval true: success.
         * @retval false: failed.
         */
        bool ReadResult(BeamformerResultMeta *meta);

        /* - Part 3.2: Array signal data Input/Output - */

        /**
         * @brief Indicate new array signal input.
         *
         * @note Tread safety.
         *
         * @retval true: new array signal input;
         * @retval false: no new array signal input.
         */
        bool HasArraySignal() const;

        /**
         * @brief Read array signal from output array signal queue.
         *
         * @note Tread safety.
         *
         * @param signalData pointer to SignalData struct to store the array signal.
         *
         * @retval true: success.
         * @retval false: failed.
         */
        bool ReadArraySignal(vuprs::SignalData *signalData);

        /* - Part 3.3: Scan power data Input/Output - */

        /**
         * @brief Indicate new scan points input.
         *
         * @note Tread safety.
         *
         * @retval true: new scan points input;
         * @retval false: no new scan points input.
         */
        bool HasScanPower() const;

        /**
         * @brief Read scan power from scan power queue.
         *
         * @note Tread safety.
         *
         * @param scanPower pointer to vector to store the scan power (in dB).
         * @param max_power_db pointer to maximum power value in dB.
         * @param min_power_db pointer to minimum power value in dB.
         *
         * @retval true: success.
         * @retval false: failed.
         */
        bool ReadScanPower(std::vector<uint16_t> *scanPower,
                           double *max_power_db,
                           double *min_power_db);

        /**
         * @brief Check config valid.
         */
        bool CheckConfigValid(const vuprs::HybridBeamformerConfig &config, std::string *info) const;

        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };
}

#endif
