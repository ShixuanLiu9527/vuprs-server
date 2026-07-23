#ifndef COLLABORATION_BEAMFORMER_H
#define COLLABORATION_BEAMFORMER_H

#include <mutex>
#include <queue>
#include <deque>
#include <atomic>
#include <condition_variable>

#include "beam_former.h"
#include "fir.h"
#include "fpga_api.h"
#include "collaboration_configs.h"

namespace vuprs
{
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
     * @note Step 1: Create CollaborationBeamformer obj.
     * @note Step 2: Call InitCollaborationBeamformer() to initialize FPGA controller and algorithm.
     * @note Step 3: Call BindBeamformer() to bind beam forming algorithm (optional, if not called, default algorithm DCRCB will be used).
     * @note Step 4: Call RUN() to start beam former.
     * @note Step 5: Call ReadResultFromQueue() to read result from queue.
     * @note Step 6: Call STOP() to stop & reset beam former.
     */
    class CollaborationBeamformer
    {
        private:

            bool configdone;

            std::vector<std::thread> threads;  /* Beam former threads */
            
            vuprs::FPGAController controller;  /* FPGA controller */
            
            std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> dmaDescriptors;  /* SG descriptors for AXI DMA */
            vuprs::AXI_DMA_SGDescriptor_Config sg_descriptorConfig;  /* SG descriptor config */

            /**
             * @brief Start beam former (hardware & algorithm).
             */
            bool StartBeamformerWithConfiguration(const vuprs::CollaborationBeamformerConfig &config);

            /**
             * @brief Reset FPGA.
             * 
             * @throw std::runtime_error
             */
            bool ResetHardwareBeamformer();

            /* Thread parameters */

            std::mutex mut;  /* Global mutex lock */

            /* Scan */

            std::mutex mut_scan_opt;  /* Scan mutex lock */
            std::vector<double> scan_alt, scan_az;  /* controlled by mut_scan_opt */
            int scan_pointsInHalf;  /* controlled by mut_scan_opt */
            double scan_waveVelocity, scan_alt_min;  /* controlled by mut_scan_opt */

            std::atomic<bool> newScanPointsInput{false};  /* scan points changed flag */
            std::mutex mut_scan_result;  /* Scan result mutex lock */
            std::condition_variable scanCV;  /* Scan condition var, controlled by mut_scan */
            std::deque<vuprs::ScanResult> scanResultQueue;  /* Scan result, controlled by mut_scan_result */
            
            /* Circular buffer interrupt */

            std::mutex mut_alg;  /* Algorithm mutex lock */
            std::condition_variable algorithmCV;  /* Algorithm interrupt condition var, [controlled by mut_alg] */

            std::deque<vuprs::SignalData> arraySignalQueue;  /* Array signal queue, [controlled by mut_alg] */

            std::atomic<bool> newArraySignalInput{false};  /* new array signal input flag */
            std::mutex mut_output_arraySignal;  /* Output array signal mutex lock */
            std::deque<vuprs::SignalData> outputArraySignalQueue;  /* Output array signal queue, controlled by mut_output_arraySignal */
            
            vuprs::FIRCalculator fir;  /* FIR algorithm, [controlled by mut_alg] (can be only used in THREAD__AlgorithmCalculation) */
            std::unique_ptr<vuprs::WidebandBeamformerTemplate> bf;  /* Beam forming algorithm, [controlled by mut_alg] (can be only used in THREAD__AlgorithmCalculation) */
            double hardwareSamplingFrequency;  /* Hardware sampling frequency, calculate by SCI register, [controlled by mut_alg] */

            /* DMA Interrupt */

            std::atomic<uint32_t> dmaCurDesc{0xFFFFFFFF};  /* current descriptor address, initialized to an invalid value */

            std::mutex mut_dma;  /* DMA Interrupt mutex lock */
            std::condition_variable dmaInterruptCV;  /* DMA Interrupt condition var, [controlled by mut_dma] */

            std::atomic<bool> newResultDataInput{false};  /* assign to outside */
            std::deque<std::vector<uint32_t>> resultQueue;  /* Result queue, [controlled by mut_dma] */

            /* Atomics */

            std::atomic<bool> system_run{false};  /* system run enable */

            std::atomic<bool> circularBufferIRQ{false};  /* Circular buffer interrupt flag */
            std::atomic<bool> dmaDescriptorIRQ{false};  /* DMA Interrupt flag */
            
            std::atomic<int> interruptWaitTime_us{0};  /* = descriptorUpdateCycle_us / 10 */
            std::atomic<int> circularBufferWaitTime_us{0};  /* = descriptorUpdateCycle_us / 5 */

            std::atomic<uint32_t> circularBufferQueueSizeMAX;  /* MAX size of circular buffer queue */
            std::atomic<uint32_t> resultQueueSizeMAX;  /* MAX size of result queue */

            std::atomic<bool> scanEnable{false};  /* Scan enable flag */
            std::atomic<bool> scanOptionsChanged{false};  /* Scan options changed flag */
            std::atomic<bool> scanOptionsInitialized{false};  /* Scan options initialized flag */

            /* Threads */

            /**
             * @brief Listen to interrupt.
             * 
             * @note If interrupt detected, dmaDescriptorIRQ <-- true.
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
             * @note If interrupt detected, circularBufferIRQ <-- true.
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

            CollaborationBeamformer();
            ~CollaborationBeamformer();

        /* ------ Part 1: Initialization ------ */

            /**
             * @brief Initialize FPGA controller & Beamforming algorithm.
             * 
             * @param fpgaConfigJson FPGA config JSON file.
             * @param bfArrayConfigJson Beam forming array config JSON file.
             * @param firConfigJson FIR filter config JSON file.
             */
            bool InitCollaborationBeamformer(const std::string &fpgaConfigJson, const std::string &bfArrayConfigJson, const std::string &firConfigJson);

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
             * @param config CollaborationBeamformerConfig struct.
             * 
             * @retval true: success.
             * @retval false: failed.
             */
            bool run(const CollaborationBeamformerConfig &config);

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
             * @param waveVelocity wave velocity (m/s). e.g. 346.0 for speed of sound in air.
             */
            bool ReDirect(double alt, double az, double waveVelocity);

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
             * @param pointsInHalf scanning points in half of the scanning area (altitude: 0-90 degree, azimuth: -180-180 degree).
             * @param alt_min minimum altitude (unit: degree) of scanning area.
             * @param waveVelocity wave velocity (m/s). e.g. 346.0 for speed of sound in air.
             */
            void ScanOptions(int pointsInHalf, double alt_min, double waveVelocity);

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
             * @param result pointer to vector to store the result.
             * 
             * @retval true: success.
             * @retval false: failed.
             */
            bool ReadResult(std::vector<uint32_t> *result);

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
             * @param maxPowerDB pointer to maximum power value in dB.
             * @param minPowerDB pointer to minimum power value in dB.
             * 
             * @retval true: success.
             * @retval false: failed.
             */
            bool ReadScanPower(std::vector<uint16_t> *scanPower, double *maxPowerDB, double *minPowerDB);

            EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };
}

#endif
