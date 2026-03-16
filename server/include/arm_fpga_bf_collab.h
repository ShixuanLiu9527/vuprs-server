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
        double fs;  /* sampling frequency (unit: Hz) */

        double bf_target__alt;  /* altitude (unit: degree) beam former pointing target */
        double bf_target__az;  /* azimuth (unit: degree) beam former pointing target */

        double bf_waveVelocity;  /* wave velocity (m/s) */

        double bf_freq__lower;  /* lower boundary of beam former work frequency (unit: Hz) */
        double bf_freq__upper;  /* upper boundary of beam former work frequency (unit: Hz) */

        int bf_cov_snapshotsWindowSize;  /* Snapshots window size (to fit covariance matrix) */
        double bf_cov_freqAverageIndex;  /* frequency average index (to fit covariance matrix) */

        uint32_t dma__bufferSize;  /* AXI DMA descriptor buffer size in bytes */
        uint32_t dma__bufferCount;  /* AXI DMA descriptor buffer count */

        uint32_t queue__circularBufferQueueSizeMAX;  /* MAX size of circular buffer queue */
        uint32_t queue__resultQueueSizeMAX;  /* MAX size of result queue */
    };

    void Set_ARM_FPGA_BF_Config_ToDefault(vuprs::ARM_FPGA_BF_Config *config);
    bool _Check_ARM_FPGA_BF_Config_Valid(vuprs::FPGAController *controller, const vuprs::ARM_FPGA_BF_Config &config);

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
            vuprs::Beamformer_DCRCB bf_dcrcb;  /* Beam forming algorithm, [controlled by mut_alg] (can be only used in THREAD__AlgorithmCalculation) */
            double hardwareSamplingFrequency;  /* Hardware sampling frequency, calculate by SCI register, [controlled by mut_alg] */

            /* DMA Interrupt */

            std::mutex mut_dma;  /* DMA Interrupt mutex lock */
            std::condition_variable dmaInterruptCV;  /* DMA Interrupt condition var, [controlled by mut_dma] */

            std::queue<std::vector<double>> resultQueue;  /* Result queue, [controlled by mut_dma] */

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
             * @brief Indicate beam former has started.
             */
            bool IS_RUN() const;

            /**
             * @brief Start beam former with configuration.
             */
            bool RUN(const ARM_FPGA_BF_Config &config);

            /**
             * @brief Change target direct of the beam former.
             */
            void ReDirect(double alt, double az, double waveVelocity);

            bool NewResultDataInput() const;

            /**
             * @brief Read result from result queue.
             */
            bool ReadResultFromQueue(std::vector<double> *result);

            /**
             * @brief Stop & reset beam former.
             */
            void STOP();

            /**
             * @brief Indicate config done.
             */
            bool ConfigDone() const;
    };
}

#endif
