#ifndef ARM_FPGA_BF_COLLAB_H
#define ARM_FPGA_BF_COLLAB_H

#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>

#include "beam_former.h"
#include "fir.h"
#include "fpga_api.h"

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

        ARM_FPGA_BF_Config() {vuprs::_Set_ARM_FPGA_BF_Config_ToDefault(this);}
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

        void Reset()
        {
            m_fs = false;

            m_bf_target__alt = false;
            m_bf_target__az = false;

            m_bf_waveVelocity = false;

            m_bf_freq__lower = false;
            m_bf_freq__upper = false;

            m_bf_cov_snapshotsWindowSize = false;
            m_bf_cov_freqAverageIndex = false;

            m_dma__bufferSize = false;
            m_dma__bufferCount = false;

            m_queue__circularBufferQueueSizeMAX = false;
            m_queue__resultQueueSizeMAX = false;
        }
    };

    void _Set_ARM_FPGA_BF_Config_ToDefault(vuprs::ARM_FPGA_BF_Config *config);
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
    class ARM_FPGA_CollaborationBeamfomer
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
            
            /* Circular buffer interrupt */

            std::mutex mut_alg;  /* Algorithm mutex lock */
            std::condition_variable algorithmCV;  /* Algorithm interrupt condition var, [controlled by mut_alg] */

            std::queue<vuprs::SignalData> arraySignalQueue;  /* Array signal queue, [controlled by mut_alg] */
            
            vuprs::FIRCalculator fir;  /* FIR algorithm, [controlled by mut_alg] (can be only used in THREAD__AlgorithmCalculation) */
            std::unique_ptr<vuprs::WidebandBeamformerTemplate> bf;  /* Beam forming algorithm, [controlled by mut_alg] (can be only used in THREAD__AlgorithmCalculation) */
            double hardwareSamplingFrequency;  /* Hardware sampling frequency, calculate by SCI register, [controlled by mut_alg] */

            /* DMA Interrupt */

            std::mutex mut_dma;  /* DMA Interrupt mutex lock */
            std::condition_variable dmaInterruptCV;  /* DMA Interrupt condition var, [controlled by mut_dma] */

            std::queue<std::vector<uint32_t>> resultQueue;  /* Result queue, [controlled by mut_dma] */

            /* Atomics */

            std::atomic<bool> system_run{false};  /* system run enable */

            std::atomic<bool> newResultDataInput{false};  /* assign to outside */

            std::atomic<bool> circularBufferIRQ{false};  /* Circular buffer interrupt flag */
            std::atomic<bool> dmaDescriptorIRQ{false};  /* DMA Interrupt flag */
            
            std::atomic<int> interruptWaitTime_us{0};  /* = descriptorUpdateCycle_us / 10 */
            std::atomic<int> circularBufferWaitTime_us{0};  /* = descriptorUpdateCycle_us / 5 */

            std::atomic<uint32_t> circularBufferQueueSizeMAX;  /* MAX size of circular buffer queue */
            std::atomic<uint32_t> resultQueueSizeMAX;  /* MAX size of result queue */

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

        public:

            ARM_FPGA_CollaborationBeamfomer();
            ~ARM_FPGA_CollaborationBeamfomer();

            /**
             * @brief Initialize FPGA controller & Beamforming algorithm.
             * 
             * @param fpgaConfigJson FPGA config JSON file.
             * @param bfArrayConfigJson Beam forming array config JSON file.
             * @param firConfigJson FIR filter config JSON file.
             */
            bool InitCollaborationBeamfomer(const std::string &fpgaConfigJson, const std::string &bfArrayConfigJson, const std::string &firConfigJon);

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
             * @brief Read result from result queue.
             * 
             * @param result pointer to vector to store the result.
             * 
             * @retval true: success.
             * @retval false: failed.
             */
            bool ReadResultFromQueue(std::vector<uint32_t> *result);

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
