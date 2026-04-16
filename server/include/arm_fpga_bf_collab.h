#ifndef ARM_FPGA_BF_COLLAB_H
#define ARM_FPGA_BF_COLLAB_H

#include <mutex>
#include <queue>
#include <deque>
#include <atomic>
#include <condition_variable>

#include "beam_former.h"
#include "fir.h"
#include "fpga_api.h"

#define DEFAULT_SCANNING_POINTS_IN_HALF 100
#define DEFAULT_SCANNING_ALTITUDE_MIN 15.0
#define DEFAULT_SCANNING_WAVE_VELOCITY 346.0

namespace vuprs
{
    struct ARM_FPGA_BF_Config
    {
        double fs;  /* sampling frequency (unit: Hz), the valid range is [10, 120000] Hz */

        double bf_target__alt;  /* altitude (unit: degree) of beam former pointing target */
        double bf_target__az;  /* azimuth (unit: degree) of beam former pointing target */

        double bf_waveVelocity;  /* wave velocity (m/s). e.g. 346.0 for speed of sound in air */

        double bf_freq__lower;  /* lower boundary of beam former work frequency (unit: Hz), the valid range is [10, 120000] Hz */
        double bf_freq__upper;  /* upper boundary of beam former work frequency (unit: Hz), the valid range is [10, 120000] Hz */

        int bf_cov_snapshotsWindowSize;  /* Snapshots window size (to fit covariance matrix) */
        double bf_cov_freqAverageIndex;  /* frequency average index (to fit covariance matrix) */

        uint32_t dma__bufferSize;  /* AXI DMA descriptor buffer size in bytes */
        uint32_t dma__bufferCount;  /* AXI DMA descriptor buffer count */

        uint32_t queue__circularBufferQueueSizeMAX;  /* MAX size of circular buffer data queue */
        uint32_t queue__resultQueueSizeMAX;  /* MAX size of result data queue */

        ARM_FPGA_BF_Config() {this->SetDefault();}

        void SetDefault();
    };

    struct ARM_FPGA_BF_Config_MASK
    {
        bool m_fs;

        bool m_bf_target__alt;
        bool m_bf_target__az;

        bool m_bf_waveVelocity;

        bool m_bf_freq__lower;
        bool m_bf_freq__upper;

        bool m_bf_cov_snapshotsWindowSize;
        bool m_bf_cov_freqAverageIndex;

        bool m_dma__bufferSize;
        bool m_dma__bufferCount;

        bool m_queue__circularBufferQueueSizeMAX;
        bool m_queue__resultQueueSizeMAX;

        ARM_FPGA_BF_Config_MASK() {this->Reset();}

        void Reset();
    };

    struct ScanningConfig
    {
        int pointsInHalf;
        double alt_min;
        bool needRegeneratePositionPoints;

        ScanningConfig(): 
        pointsInHalf(DEFAULT_SCANNING_POINTS_IN_HALF), 
        alt_min(DEFAULT_SCANNING_ALTITUDE_MIN), 
        needRegeneratePositionPoints(true) {}
    };

    struct ScanResult
    {
        std::vector<uint16_t> scanResult;  /* scan result in power, unit: dB */
        double minPowerDB;  /* minimum power in dB for scan result */
        double maxPowerDB;  /* maximum power in dB for scan result */
    };

    bool _Check_ARM_FPGA_BF_Config_Valid(vuprs::FPGAController *controller, const vuprs::ARM_FPGA_BF_Config &config);

    /**
     * @brief Merge newConfig to config according to configMask.
     * 
     * @param config original config, will be updated after merging.
     * @param newConfig new config, will be merged to original config according to configMask
     * @param configMask config mask, indicate which field in newConfig will be merged to original config. true: merge, false: not merge.
     */
    void Merge_ARM_FPGA_BF_Config(vuprs::ARM_FPGA_BF_Config *config, const vuprs::ARM_FPGA_BF_Config &newConfig, const vuprs::ARM_FPGA_BF_Config_MASK &configMask);

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
     * @note Step 1: Create ARM_FPGA_CollaborationBeamformer obj.
     * @note Step 2: Call InitCollaborationBeamformer() to initialize FPGA controller and algorithm.
     * @note Step 3: Call BindBeamformer() to bind beam forming algorithm (optional, if not called, default algorithm DCRCB will be used).
     * @note Step 4: Call RUN() to start beam former.
     * @note Step 5: Call ReadResultFromQueue() to read result from queue.
     * @note Step 6: Call STOP() to stop & reset beam former.
     */
    class ARM_FPGA_CollaborationBeamformer
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
            bool StartBeamformerWithConfiguration(const ARM_FPGA_BF_Config &config);

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

            ARM_FPGA_CollaborationBeamformer();
            ~ARM_FPGA_CollaborationBeamformer();

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
             * @brief Indicate beam former has started.
             * 
             * @retval true: beam former is running;
             * @retval false: beam former is not running.
             */
            bool IS_RUN() const;

            /**
             * @brief Start beam former with configuration.
             * 
             * @param config ARM_FPGA_BF_Config struct.
             * 
             * @retval true: success.
             * @retval false: failed.
             */
            bool RUN(const ARM_FPGA_BF_Config &config);

            /**
             * @brief Change target direct of the beam former.
             * 
             * @note The function can be called when the beam former is running, 
             * @note and the direction of beamformer will be changed in real time.
             * 
             * @param alt altitude (unit: degree) of beam former pointing target.
             * @param az azimuth (unit: degree) of beam former pointing target.
             * @param waveVelocity wave velocity (m/s). e.g. 346.0 for speed of sound in air.
             */
            bool ReDirect(double alt, double az, double waveVelocity);

            /**
             * @brief Indicate new result data input.
             * 
             * @retval true: new result data input;
             * @retval false: no new result data input.
             */
            bool NewResultDataInput() const;

            /**
             * @brief Indicate new array signal input.
             * 
             * @retval true: new array signal input;
             * @retval false: no new array signal input.
             */
            bool NewArraySignalInput() const;

            /**
             * @brief Indicate new scan points input.
             * 
             * @retval true: new scan points input;
             * @retval false: no new scan points input.
             */
            bool NewScanPowerInput() const;

            /**
             * @brief Set scan enable.
             * 
             * @param enable true: enable scan; false: disable scan.
             */
            void ScanSwitch(bool enable);
            bool ScanSwitch() const;

            /**
             * @brief Set scan options.
             * 
             * @param pointsInHalf scanning points in half of the scanning area (altitude: 0-90 degree, azimuth: -180-180 degree).
             * @param alt_min minimum altitude (unit: degree) of scanning area.
             * @param waveVelocity wave velocity (m/s). e.g. 346.0 for speed of sound in air.
             */
            void ScanOptions(int pointsInHalf, double alt_min, double waveVelocity);

            /**
             * @brief Read result from result queue.
             * 
             * @param result pointer to vector to store the result.
             * 
             * @retval true: success.
             * @retval false: failed.
             */
            bool ReadResultFromQueue(std::vector<uint32_t> *result);

            /**
             * @brief Read array signal from output array signal queue.
             * 
             * @param signalData pointer to SignalData struct to store the array signal.
             * 
             * @retval true: success.
             * @retval false: failed.
             */
            bool ReadArraySignalFromQueue(vuprs::SignalData *signalData);

            /**
             * @brief Read scan power from scan power queue.
             * 
             * @param scanPower pointer to vector to store the scan power (in dB).
             * @param maxPowerDB pointer to maximum power value in dB.
             * @param minPowerDB pointer to minimum power value in dB.
             * 
             * @retval true: success.
             * @retval false: failed.
             */
            bool ReadScanPowerFromQueue(std::vector<uint16_t> *scanPower, double *maxPowerDB, double *minPowerDB);

            /**
             * @brief Stop & reset beam former.
             */
            void STOP();

            /**
             * @brief Indicate config done.
             * 
             * @retval true: config done;
             * @retval false: config not complete.
             */
            bool ConfigDone() const;

            EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };
}

#endif
