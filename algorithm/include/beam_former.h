#ifndef BEAM_FORMER_H
#define BEAM_FORMER_H

#include "beam_former_template.h"

#define DEFAULT_MVDR_FRAME_WINDOW_LENGTH 50

namespace vuprs
{
    class BeamFormerCBF: public BeamFormerTemplate
    {
        public:

            void GetOutputSignal(std::vector<std::complex<double>> *outputSignal) override;
    };

    class BeamFormerMVDR: public BeamFormerTemplate
    {
        private:

            int currentSignalPoints = -1;  /* signal points */
            int windowSize = DEFAULT_MVDR_FRAME_WINDOW_LENGTH;  /* window size */

            Eigen::Matrix<Eigen::dcomplex, -1, -1> currentSignalMatrix;
            std::vector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> averageCovarianceMatrixList;
            std::vector<std::vector<Eigen::Matrix<Eigen::dcomplex, -1, -1>>> frameCovarianceMatrixListWindow;

            void CalculateSignalCovarianceMatrixInCurrentFrame();

            void CalculateAverageCovarianceMatrix();

            void ResetCovarianceMatrixParam();

        public:

            BeamFormerMVDR();

            void GetOutputSignal(std::vector<std::complex<double>> *outputSignal) override;

            void SetWindowSize(int newSize = -1);
    };
}

#endif
