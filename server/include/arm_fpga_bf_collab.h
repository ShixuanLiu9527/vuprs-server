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

        double bf_freq__lower;  /* lower boundary of beam former work frequency (unit: Hz) */
        double bf_freq__upper;  /* upper boundary of beam former work frequency (unit: Hz) */

        int bf_cov_snapshotsWindowSize;  /* Snapshots window size (to fit covariance matrix) */
        double bf_cov_freqAverageIndex;  /* frequency average index (to fit covariance matrix) */

        uint32_t dma__bufferSize;  /* AXI DMA descriptor buffer size */
        uint32_t dma__bufferCount;  /* AXI DMA descriptor buffer count */
    };

    class ARM_FPGA_CollaborationBeamfomer
    {
        private:

            vuprs::FPGAController controller;
            vuprs::FIRCalculator fir;
            vuprs::Beamformer_DCRCB bf_dcrcb;
            vuprs::Beamformer_CBF bf_cbf;

            vuprs::SignalData multichannelSignal;
            std::vector<double> bfResult;

            std::vector<vuprs::AXI_DMA_ScatterGatherDescriptor> dmaDescriptors;

        public:

            ARM_FPGA_CollaborationBeamfomer();
            ~ARM_FPGA_CollaborationBeamfomer();

            /**
             * @brief Initialize FPGA controller & Beamforming algorithm.
             * 
             * @param fpgaConfigJson FPGA config JSON file.
             * @param bfArrayConfigJson Beam forming array config Json.
             */
            bool InitCollaborationBeamfomer(const std::string &fpgaConfigJson, const std::string &bfArrayConfigJson);

            /**
             * @brief Reset FPGA.
             * 
             * @throw std::runtime_error
             */
            bool ResetHardwareBeamformer();

            /**
             * @brief Start beam forming.
             */
            bool StartBeamforming(const ARM_FPGA_BF_Config &config);
    };
}

#endif
