#ifndef ARM_FPGA_BF_COLLAB_H
#define ARM_FPGA_BF_COLLAB_H

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

        uint32_t dma__bufferSize;  /* AXI DMA descriptor buffer size */
        uint32_t dma__bufferCount;  /* AXI DMA descriptor buffer count */
    };

    void Set_ARM_FPGA_BF_Config_ToDefault(vuprs::ARM_FPGA_BF_Config *config);
    bool _Check_ARM_FPGA_BF_Config_Valid(const vuprs::ARM_FPGA_BF_Config &config);

    class ARM_FPGA_CollaborationBeamfomer
    {
        private:

            bool configdone;
            bool beamformerStarted;
            bool firstCoefficientsIssued;
            
            double hardwareSamplingFrequency;  /* Hardware sampling frequency, calculate by SCI register */

            vuprs::FPGAController controller;  /* FPGA controller */
            vuprs::FIRCalculator fir;  /* FIR algorithm */
            vuprs::Beamformer_DCRCB bf_dcrcb;  /* Beam forming algorithm */
            
            vuprs::SignalData multichannelSignal;  /* signal data (from circular buffer) */
            std::vector<double> bfResult;  /* Beam forming output (from DDR) */

            std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> dmaDescriptors;  /* SG descriptors for AXI DMA */
            vuprs::AXI_DMA_SGDescriptor_Config sg_descriptorConfig;  /* SG descriptor config */

            std::vector<int> predelayCount;  /* Channel predelay count */
            std::vector<double> predelayTime;  /* Channel predelay time */
            std::vector<std::string> channelName;  /* Corrsponding name of channel */

            Eigen::Matrix<Eigen::dcomplex, -1, -1> firExpectedFrequencyResponse;  /* Expected frequency response of FIR filter bank */
            std::vector<std::vector<double>> firCoefficients;  /* Coefficient of FIR filter bank */

        public:

            ARM_FPGA_CollaborationBeamfomer();
            ~ARM_FPGA_CollaborationBeamfomer();

            /**
             * @brief Initialize FPGA controller & Beamforming algorithm.
             * 
             * @param fpgaConfigJson FPGA config JSON file.
             * @param bfArrayConfigJson Beam forming array config JSON file.
             * @param firConfigJon FIR filter config JSON file.
             */
            bool InitCollaborationBeamfomer(const std::string &fpgaConfigJson, const std::string &bfArrayConfigJson, const std::string &firConfigJon);

            /**
             * @brief Reset FPGA.
             * 
             * @throw std::runtime_error
             */
            bool ResetHardwareBeamformer();

            /**
             * @brief Start beam former (hardware & algorithm).
             */
            bool StartBeamformer(const ARM_FPGA_BF_Config &config);

            /**
             * @brief Indicate beam former has started.
             */
            bool BeamformerHasStarted() const;

            /**
             * @brief Get signal from Circular Buffer and issue FIR filter coefficients to FIR.
             * 
             * @note STEP 1: Check if circular buffer refreshed.
             * @note STEP 2: Read signal from circular buffer.
             * @note STEP 3: Run algorithms.
             * @note STEP 4: Calculate FIR filter coefficients.
             * @note STEP 5: Issue coefficients to FIR filter bank.
             */
            bool GetSignalAndIssueFIRFilterCoefficients();

            /**
             * @brief Indicate config done.
             */
            bool ConfigDone() const;

            /**
             * @brief Get all SG descriptors of AXI DMA.
             */
            void GetDMADescriptor(std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> *output) const;

            /**
             * @brief Indicate if beam former have valid output.
             * 
             * @note When the first set of effective coefficients of FIR filter bank is 
             *       successfully configured, calling this function will return TRUE.
             */
            bool BeamFormerHaveValidOutput() const;
    };
}

#endif
