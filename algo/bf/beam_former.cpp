#include <cmath>
#include "algo/bf/beam_former.h"

/* ------------------------------------------------------------------------------ */
/* ---------------------------------- DCRCB ------------------------------------- */
/* ------------------------------------------------------------------------------ */

vuprs::Beamformer_DCRCB::Beamformer_DCRCB()
{
}

vuprs::Beamformer_DCRCB::~Beamformer_DCRCB()
{
}

void vuprs::Beamformer_DCRCB::CalculateBeamformingForOneFreq(int freq_index)
{
    /* Read */
    Eigen::Ref<Eigen::Matrix<Eigen::dcomplex, -1, 1>> ps = this->steering_vectors.col(freq_index);
    const Eigen::Matrix<Eigen::dcomplex, -1, -1> &R = this->estimate_cov_matrix[freq_index];
    double freq = this->signal_frequency_list(freq_index);
    double e0 = this->array.CalculateSteeringVectorErrorRadius(freq);
    int M = this->array.size();
    double denominator = (double)M - e0 / 2.0; /* = (M - e0/2) */
    RUNTIME_CHECK(denominator > 0.0, "bf", " Invalid steering error radius.");
    double rho = (double)M / (denominator * denominator); /* M / (M - e0 / 2)^2 */
    double sqrt_M_rho = std::sqrt((double)M * rho);       /* sqrt(M * rho) */

    /* STEP 1: Eigenvalue decomposition for covariance matrix R */
    vuprs::EigenvalueDecomposition(R, &gamma[freq_index], &U[freq_index]);
    U_H[freq_index] = U[freq_index].adjoint();
    const Eigen::Matrix<double, -1, 1> &g = gamma[freq_index];
    double min_eig = g.minCoeff();
    double max_eig = g.maxCoeff();
    zs[freq_index] = U_H[freq_index] * ps;

    /* STEP 2: Solve f(lambda) = rho with a scalar loop (no temporaries, no pow). */
    const Eigen::Matrix<double, -1, 1> inv_gamma = g.cwiseInverse(); /* gamma.^-1 (element wise) */
    const Eigen::Matrix<double, -1, 1> zs2 = zs[freq_index].cwiseAbs2();
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
    iter.lower_region = -(1.0 / max_eig);                                              /* -1/max(eig) */
    iter.upper_region = ((1.0 / min_eig) - sqrt_M_rho / max_eig) / (sqrt_M_rho - 1.0); /* (1/min(eig) - sqrt(M*rho)/max(eig)) / (sqrt(M*rho) - 1) */
    iter.func = func_lambda;
    iter.lower_region = std::nextafter(iter.lower_region, iter.upper_region);
    RUNTIME_CHECK(std::isfinite(iter.lower_region) && std::isfinite(iter.upper_region) && iter.upper_region > iter.lower_region,
                  "bf", " Invalid lambda region.");
    /* - Iteration */
    double lambda = 0.0; /* result lambda */
    RUNTIME_CHECK(vuprs::BisectionIteration1D(iter, &lambda), "bf", " Lambda iteration failed.");

    /** STEP 3: Get ps_hat.
     * Apply (gamma.^-1 + lambda .* I).^-1 without materializing it: (U * D * U.H) * x = U * (D .* (U.H * x)).
     */
    Eigen::Matrix<double, -1, 1> inv_gamma_lambda = (inv_gamma.array() + lambda).inverse().matrix();
    Eigen::Matrix<Eigen::dcomplex, -1, 1> tmp = U_H[freq_index] * ps;
    for (int m = 0; m < M; ++m)
        tmp(m) *= inv_gamma_lambda(m);
    invR_lambdaI_inv_mul_ps[freq_index] = U[freq_index] * tmp;
    double ps_hat_denominator = (ps.adjoint() * invR_lambdaI_inv_mul_ps[freq_index])(0, 0).real();
    RUNTIME_CHECK(std::abs(ps_hat_denominator) > 0.0, "bf", " Invalid ps_hat denominator.");
    ps_hat[freq_index] = invR_lambdaI_inv_mul_ps[freq_index] * (denominator / ps_hat_denominator);

    /* STEP 4: Get w. R.^-1 * ps_hat = U * (gamma.^-1 .* (U.H * ps_hat)). */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> tmp_w = U_H[freq_index] * ps_hat[freq_index];
    for (int m = 0; m < M; ++m)
        tmp_w(m) *= inv_gamma(m);
    invR_ps_hat[freq_index] = U[freq_index] * tmp_w;
    double w_denominator = (ps_hat[freq_index].adjoint() * invR_ps_hat[freq_index])(0, 0).real();
    RUNTIME_CHECK(std::abs(w_denominator) > 0.0, "bf", " Invalid weight denominator.");
    w[freq_index] = invR_ps_hat[freq_index] / w_denominator;
    /* Dump */
    this->result_weight_vectors.col(freq_index) = w[freq_index];
}

/* ------------------------------------------------------------------------------ */
/* ------------------------------------ CBF ------------------------------------- */
/* ------------------------------------------------------------------------------ */

vuprs::Beamformer_CBF::Beamformer_CBF()
{
}

vuprs::Beamformer_CBF::~Beamformer_CBF()
{
}

void vuprs::Beamformer_CBF::CalculateBeamformingForOneFreq(int freq_index)
{
    Eigen::Matrix<Eigen::dcomplex, -1, 1> ps = this->steering_vectors.col(freq_index); /* ps */
    double M = this->array.size();                                                     /* M */
    this->result_weight_vectors.col(freq_index) = ps / M;
}

/* ------------------------------------------------------------------------------ */
/* ------------------------------------ MVDR ------------------------------------ */
/* ------------------------------------------------------------------------------ */

vuprs::Beamformer_MVDR::Beamformer_MVDR()
{
}

vuprs::Beamformer_MVDR::~Beamformer_MVDR()
{
}

void vuprs::Beamformer_MVDR::CalculateBeamformingForOneFreq(int freq_index)
{
    /* Read */
    ps[freq_index] = this->steering_vectors.col(freq_index);
    R[freq_index] = this->estimate_cov_matrix[freq_index];
    /* Compute */
    invR[freq_index] = R[freq_index].inverse();
    invR_ps[freq_index] = invR[freq_index] * ps[freq_index];
    w[freq_index] = invR_ps[freq_index] / (ps[freq_index].adjoint() * invR_ps[freq_index])(0, 0);
    /* Dump */
    this->result_weight_vectors.col(freq_index) = w[freq_index];
}
