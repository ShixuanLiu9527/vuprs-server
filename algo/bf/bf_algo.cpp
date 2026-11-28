#include <algorithm>
#include <cmath>
#include <thread>
#include "algo/bf/bf_algo.h"
#include "algo/bf/bf_array.h"
#include "algo/utils/math_utils.h"

/* ------------------------------------------------------------------------------ */
/* ---------------------------------- DCRCB ------------------------------------- */
/* ------------------------------------------------------------------------------ */

vuprs::BF_DCRCB::BF_DCRCB()
{
}

vuprs::BF_DCRCB::~BF_DCRCB()
{
}

void vuprs::BF_DCRCB::Prepare(int frequency_count, int element_count)
{
    (void)element_count;
    this->gamma.resize(frequency_count);
    this->U.resize(frequency_count);
    this->U_H.resize(frequency_count);
    this->zs.resize(frequency_count);
    this->invR_lambdaI_inv_mul_ps.resize(frequency_count);
    this->ps_hat.resize(frequency_count);
    this->invR_ps_hat.resize(frequency_count);
    this->w.resize(frequency_count);
    /* Solver cache: one solver per worker chunk (max = thread count). */
    const size_t worker_count = std::max<size_t>(size_t{1}, std::thread::hardware_concurrency());
    if (this->eig_solver_cache.size() != worker_count)
        this->eig_solver_cache.resize(worker_count);
}

void vuprs::BF_DCRCB::CalculateOneFrequency(int frequency_index,
                                            int chunk_id,
                                            const vuprs::BeamFormingArray &array,
                                            const Eigen::Matrix<Eigen::dcomplex, -1, -1> &steering_vectors,
                                            const vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> &covariance_matrices,
                                            const Eigen::Matrix<double, -1, 1> &signal_frequency_list,
                                            Eigen::Matrix<Eigen::dcomplex, -1, -1> *result_weight_vectors)
{
    /* Read */
    const auto ps = steering_vectors.col(frequency_index);
    const Eigen::Matrix<Eigen::dcomplex, -1, -1> &R = covariance_matrices[frequency_index];
    double freq = signal_frequency_list(frequency_index);
    double e0 = array.CalculateSteeringVectorErrorRadius(freq);
    int M = static_cast<int>(array.size());
    double denominator = (double)M - e0 / 2.0; /* = (M - e0/2) */
    RUNTIME_CHECK(denominator > 0.0, "bf", " Invalid steering error radius.");
    double rho = (double)M / (denominator * denominator); /* M / (M - e0 / 2)^2 */
    double sqrt_M_rho = std::sqrt((double)M * rho);       /* sqrt(M * rho) */

    /* STEP 1: Eigenvalue decomposition for covariance matrix R. */
    /* Reuse the per-chunk solver to avoid per-bin allocations (one solver per worker chunk, never shared by two tasks). */
    RUNTIME_CHECK(chunk_id >= 0, "bf", " Invalid chunk id.");
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXcd> &solver = this->eig_solver_cache[chunk_id];
    solver.compute(R);
    RUNTIME_CHECK(solver.info() == Eigen::Success, "bf", "Failed to solving eigenvalues");
    gamma[frequency_index] = solver.eigenvalues();
    U[frequency_index] = solver.eigenvectors();
    U_H[frequency_index] = U[frequency_index].adjoint();
    const Eigen::Matrix<double, -1, 1> &g = gamma[frequency_index];
    double min_eig = g.minCoeff();
    double max_eig = g.maxCoeff();
    zs[frequency_index] = U_H[frequency_index] * ps;

    /* STEP 2: Solve f(lambda) = rho with a scalar loop (no temporaries, no pow). */
    const Eigen::Matrix<double, -1, 1> inv_gamma = g.cwiseInverse(); /* gamma.^-1 (element wise) */
    const Eigen::Matrix<double, -1, 1> zs2 = zs[frequency_index].cwiseAbs2();
    auto func_lambda = [&inv_gamma, &zs2, rho, M](double val) -> double
    {
        double s1 = 0.0, s2 = 0.0;
        for (int m = 0; m < M; ++m)
        {
            double p = 1.0 / (inv_gamma(m) + val);
            double z2 = zs2(m);
            s1 += z2 * p;
            s2 += z2 * p * p;
        }
        return s2 / (s1 * s1) - rho;
    };
    /* - Generate range. The lower bound is a pole of f(lambda); nudge it inward. */
    vuprs::IterationConfig iter;
    vuprs::SetIterationConfigDefault(&iter);
    iter.region_lo = -(1.0 / max_eig);                                              /* -1/max(eig) */
    iter.region_hi = ((1.0 / min_eig) - sqrt_M_rho / max_eig) / (sqrt_M_rho - 1.0); /* (1/min(eig) - sqrt(M*rho)/max(eig)) / (sqrt(M*rho) - 1) */
    iter.func = func_lambda;
    iter.region_lo = std::nextafter(iter.region_lo, iter.region_hi);
    RUNTIME_CHECK(std::isfinite(iter.region_lo) && std::isfinite(iter.region_hi) && iter.region_hi > iter.region_lo,
                  "bf", " Invalid lambda region.");
    /* - Iteration */
    double lambda = 0.0; /* result lambda */
    RUNTIME_CHECK(vuprs::BisectionIteration1D(iter, &lambda), "bf", " Lambda iteration failed.");

    /** STEP 3: Get ps_hat.
     * Apply (gamma.^-1 + lambda .* I).^-1 without materializing it: (U * D * U.H) * x = U * (D .* (U.H * x)).
     */
    Eigen::Matrix<double, -1, 1> inv_gamma_lambda = (inv_gamma.array() + lambda).inverse().matrix();
    Eigen::Matrix<Eigen::dcomplex, -1, 1> tmp = U_H[frequency_index] * ps;
    for (int m = 0; m < M; ++m)
        tmp(m) *= inv_gamma_lambda(m);
    invR_lambdaI_inv_mul_ps[frequency_index] = U[frequency_index] * tmp;
    double ps_hat_denominator = (ps.adjoint() * invR_lambdaI_inv_mul_ps[frequency_index])(0, 0).real();
    RUNTIME_CHECK(std::abs(ps_hat_denominator) > 0.0, "bf", " Invalid ps_hat denominator.");
    ps_hat[frequency_index] = invR_lambdaI_inv_mul_ps[frequency_index] * (denominator / ps_hat_denominator);

    /* STEP 4: Get w. R.^-1 * ps_hat = U * (gamma.^-1 .* (U.H * ps_hat)). */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> tmp_w = U_H[frequency_index] * ps_hat[frequency_index];
    for (int m = 0; m < M; ++m)
        tmp_w(m) *= inv_gamma(m);
    invR_ps_hat[frequency_index] = U[frequency_index] * tmp_w;
    double w_denominator = (ps_hat[frequency_index].adjoint() * invR_ps_hat[frequency_index])(0, 0).real();
    RUNTIME_CHECK(std::abs(w_denominator) > 0.0, "bf", " Invalid weight denominator.");
    w[frequency_index] = invR_ps_hat[frequency_index] / w_denominator;
    /* Dump */
    (*result_weight_vectors).col(frequency_index) = w[frequency_index];
}

/* ------------------------------------------------------------------------------ */
/* ------------------------------------ CBF ------------------------------------- */
/* ------------------------------------------------------------------------------ */

vuprs::BF_CBF::BF_CBF()
{
}

vuprs::BF_CBF::~BF_CBF()
{
}

void vuprs::BF_CBF::Prepare(int frequency_count, int element_count)
{
    (void)frequency_count;
    (void)element_count;
}

void vuprs::BF_CBF::CalculateOneFrequency(int frequency_index,
                                          int chunk_id,
                                          const vuprs::BeamFormingArray &array,
                                          const Eigen::Matrix<Eigen::dcomplex, -1, -1> &steering_vectors,
                                          const vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> &covariance_matrices,
                                          const Eigen::Matrix<double, -1, 1> &signal_frequency_list,
                                          Eigen::Matrix<Eigen::dcomplex, -1, -1> *result_weight_vectors)
{
    (void)chunk_id; /* CBF has no per-worker cache */
    (void)covariance_matrices;
    (void)signal_frequency_list;
    double M = static_cast<double>(array.size()); /* M */
    (*result_weight_vectors).col(frequency_index) = steering_vectors.col(frequency_index) / M;
}

/* ------------------------------------------------------------------------------ */
/* ------------------------------------ MVDR ------------------------------------ */
/* ------------------------------------------------------------------------------ */

vuprs::BF_MVDR::BF_MVDR()
{
}

vuprs::BF_MVDR::~BF_MVDR()
{
}

void vuprs::BF_MVDR::Prepare(int frequency_count, int element_count)
{
    (void)element_count;
    this->ps.resize(frequency_count);
    this->R.resize(frequency_count);
    this->invR.resize(frequency_count);
    this->invR_ps.resize(frequency_count);
    this->w.resize(frequency_count);
}

void vuprs::BF_MVDR::CalculateOneFrequency(int frequency_index,
                                           int chunk_id,
                                           const vuprs::BeamFormingArray &array,
                                           const Eigen::Matrix<Eigen::dcomplex, -1, -1> &steering_vectors,
                                           const vuprs::AlignedEigenVector<Eigen::Matrix<Eigen::dcomplex, -1, -1>> &covariance_matrices,
                                           const Eigen::Matrix<double, -1, 1> &signal_frequency_list,
                                           Eigen::Matrix<Eigen::dcomplex, -1, -1> *result_weight_vectors)
{
    (void)chunk_id; /* MVDR has no per-worker cache */
    (void)array;
    (void)signal_frequency_list;
    /* Read */
    ps[frequency_index] = steering_vectors.col(frequency_index);
    R[frequency_index] = covariance_matrices[frequency_index];
    /* Compute */
    invR[frequency_index] = R[frequency_index].inverse();
    invR_ps[frequency_index] = invR[frequency_index] * ps[frequency_index];
    w[frequency_index] = invR_ps[frequency_index] / (ps[frequency_index].adjoint() * invR_ps[frequency_index])(0, 0);
    /* Dump */
    (*result_weight_vectors).col(frequency_index) = w[frequency_index];
}
