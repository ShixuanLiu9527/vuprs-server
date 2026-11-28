#ifndef BEAM_FORMING_ALGO_H
#define BEAM_FORMING_ALGO_H

#include <Eigen/Dense>
#include "misc/aligned_vec.h"

namespace vuprs
{
    class BeamFormingArray;

    /**
     * @brief Pluggable beam forming algorithm.
     *
     * @note An algorithm only owns its private caches; the array, signal data,
     * @note covariance matrices and scan state live in the persistent BeamFormingEngine.
     * @note Swapping the algorithm therefore keeps the covariance matrix warm.
     */
    class BeamFormingAlgo
    {
    public:
        virtual ~BeamFormingAlgo() = default;

        /**
         * @brief Prepare per-frequency (and per-worker) caches.
         *
         * @param frequency_count frequency bin count (= signal points / 2 + 1).
         * @param element_count array element count (M).
         */
        virtual void Prepare(int frequency_count, int element_count) = 0;

        /**
         * @brief Calculate the weight vector for one frequency bin.
         *
         * @param frequency_index frequency bin index.
         * @param chunk_id unique worker chunk id (for per-worker caches, -1 if none).
         * @param array beam forming array (element count & steering error radius).
         * @param steering_vectors M x F steering vectors.
         * @param covariance_matrices F covariance matrices (each M x M).
         * @param signal_frequency_list F bin frequencies (Hz).
         * @param result_weight_vectors [output] M x F weight vectors.
         */
        virtual void CalculateOneFrequency(int frequency_index,
                                           int chunk_id,
                                           const vuprs::BeamFormingArray &array,
                                           const Eigen::Matrix<Eigen::dcomplex, -1, -1> &steering_vectors,
                                           const vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> &covariance_matrices,
                                           const Eigen::Matrix<double, -1, 1> &signal_frequency_list,
                                           Eigen::Matrix<Eigen::dcomplex, -1, -1> *result_weight_vectors) = 0;
    };

    class BF_DCRCB : public vuprs::BeamFormingAlgo
    {
    private:
        vuprs::AlignedEigenVector<Eigen::Matrix<double, -1, 1>> gamma;                            /* matrix gamma (Note: not diag) */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> U;                      /* matrix U, R = U * gamma * U.H */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> U_H;                    /* matrix U.H, R = U * gamma * U.H */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> zs;                      /* zs = U.H @ ps */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> invR_lambdaI_inv_mul_ps; /* ((R.-1 + lambda * I).-1) * ps */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> ps_hat;                  /* (M - e0 / 2) * [((R.-1 + lambda * I).-1) * ps] / [ps.H * ((R.-1 + lambda * I).-1) * ps] */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> invR_ps_hat;             /* R.-1 * ps.hat */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> w;                       /* result weight = (R.-1 * ps_hat) / (ps_hat.H * R.-1 * ps_hat) */
        std::vector<Eigen::SelfAdjointEigenSolver<Eigen::MatrixXcd>> eig_solver_cache;            /* One preallocated solver per worker chunk */

    public:
        BF_DCRCB();
        ~BF_DCRCB() override;

        void Prepare(int frequency_count, int element_count) override;

        void CalculateOneFrequency(int frequency_index,
                                   int chunk_id,
                                   const vuprs::BeamFormingArray &array,
                                   const Eigen::Matrix<Eigen::dcomplex, -1, -1> &steering_vectors,
                                   const vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> &covariance_matrices,
                                   const Eigen::Matrix<double, -1, 1> &signal_frequency_list,
                                   Eigen::Matrix<Eigen::dcomplex, -1, -1> *result_weight_vectors) override;

        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };

    class BF_CBF : public vuprs::BeamFormingAlgo
    {
    public:
        BF_CBF();
        ~BF_CBF() override;

        void Prepare(int frequency_count, int element_count) override;

        void CalculateOneFrequency(int frequency_index,
                                   int chunk_id,
                                   const vuprs::BeamFormingArray &array,
                                   const Eigen::Matrix<Eigen::dcomplex, -1, -1> &steering_vectors,
                                   const vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> &covariance_matrices,
                                   const Eigen::Matrix<double, -1, 1> &signal_frequency_list,
                                   Eigen::Matrix<Eigen::dcomplex, -1, -1> *result_weight_vectors) override;

        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };

    class BF_MVDR : public vuprs::BeamFormingAlgo
    {
    private:
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> ps;      /* steering vector ps */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> R;      /* covariance matrix R */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> invR;   /* R.-1 = U * GAMMA.-1 * U.H */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> invR_ps; /* R.-1 * ps */
        vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, 1>> w;       /* result weight = (R.-1 * ps) / (ps.H * R.-1 * ps) */

    public:
        BF_MVDR();
        ~BF_MVDR() override;

        void Prepare(int frequency_count, int element_count) override;

        void CalculateOneFrequency(int frequency_index,
                                   int chunk_id,
                                   const vuprs::BeamFormingArray &array,
                                   const Eigen::Matrix<Eigen::dcomplex, -1, -1> &steering_vectors,
                                   const vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> &covariance_matrices,
                                   const Eigen::Matrix<double, -1, 1> &signal_frequency_list,
                                   Eigen::Matrix<Eigen::dcomplex, -1, -1> *result_weight_vectors) override;

        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };
}

#endif
