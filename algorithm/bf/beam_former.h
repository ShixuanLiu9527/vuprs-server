#ifndef BEAM_FORMER_H
#define BEAM_FORMER_H

#include "algorithm/bf/beam_former_template.h"

namespace vuprs
{
    class Beamformer_DCRCB : public vuprs::WidebandBeamformerTemplate
    {
    private:
        vuprs::AlignedEigenVector<Eigen::Matrix<double, -1, 1>> gamma;                            /* matrix gamma (Note: not diag) */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> U;                      /* matrix U, R = U * gamma * U.H */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> U_H;                    /* matrix U.H, R = U * gamma * U.H */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> zs;                      /* zs = U.H @ ps */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> inv_gamma_lambda_I;     /* (gamma.-1 + lambda * I).-1 */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> invR_lambdaI_inv_mul_ps; /* ((R.-1 + lambda * I).-1) * ps */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> ps_hat;                  /* (M - e0 / 2) * [((R.-1 + lambda * I).-1) * ps] / [ps.H * ((R.-1 + lambda * I).-1) * ps] */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> invR;                   /* R.-1 = U * GAMMA.-1 * U.H */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> invR_ps_hat;             /* R.-1 * ps.hat */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> w;                       /* result weight = (R.-1 * ps_hat) / (ps_hat.H * R.-1 * ps_hat) */
    protected:
        void CalculateBeamformingForOneFreq(int freq_index) override;
        void PrepareCalculationCache(int freq_nums) override
        {
            this->gamma.resize(freq_nums);
            this->U.resize(freq_nums);
            this->U_H.resize(freq_nums);
            this->zs.resize(freq_nums);
            this->inv_gamma_lambda_I.resize(freq_nums);
            this->invR_lambdaI_inv_mul_ps.resize(freq_nums);
            this->ps_hat.resize(freq_nums);
            this->invR.resize(freq_nums);
            this->invR_ps_hat.resize(freq_nums);
            this->w.resize(freq_nums);
        }

    public:
        Beamformer_DCRCB();
        ~Beamformer_DCRCB();
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };

    class Beamformer_CBF : public vuprs::WidebandBeamformerTemplate
    {
    private:
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> ps; /* steering vector ps */

    protected:
        void CalculateBeamformingForOneFreq(int freq_index) override;
        void PrepareCalculationCache(int freq_nums) override
        {
            this->ps.resize(freq_nums);
        }

    public:
        Beamformer_CBF();
        ~Beamformer_CBF();
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };

    class Beamformer_MVDR : public vuprs::WidebandBeamformerTemplate
    {
    private:
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> ps;      /* steering vector ps */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> R;      /* covariance matrix R */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> invR;   /* R.-1 = U * GAMMA.-1 * U.H */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> invR_ps; /* R.-1 * ps */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> w;       /* result weight = (R.-1 * ps) / (ps.H * R.-1 * ps) */

    protected:
        void CalculateBeamformingForOneFreq(int freq_index) override;
        void PrepareCalculationCache(int freq_nums) override
        {
            this->ps.resize(freq_nums);
            this->R.resize(freq_nums);
            this->invR.resize(freq_nums);
            this->invR_ps.resize(freq_nums);
            this->w.resize(freq_nums);
        }

    public:
        Beamformer_MVDR();
        ~Beamformer_MVDR();
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };
}

#endif
