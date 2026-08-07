#include "algorithm/bf/beam_former.h"

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
    Eigen::Ref<Eigen::Matrix<Eigen::dcomplex, -1, -1>> R = this->estimate_cov_matrix[freq_index];
    double fs = this->signal_frequency_list(freq_index);
    double e0 = this->array.CalculateSteeringVectorErrorRadius(fs);
    int M = this->array.size();
    double max_eig;                                          /* max(eigenvalues) */
    double min_eig;                                          /* min(eigenvalues) */
    double rho = (double)M / pow((double)M - e0 / 2.0, 2.0); /* M / (M - e0 / 2)^2 */
    double sqrt_M_rho = sqrt((double)M * rho);               /* sqrt(M * rho) */
    /* STEP 1: Eigenvalue decomposition for covariance matrix R */
    vuprs::EigenvalueDecomposition(R, &gamma[freq_index], &U[freq_index]);
    U_H[freq_index] = U[freq_index].adjoint();
    min_eig = gamma[freq_index].minCoeff();
    max_eig = gamma[freq_index].maxCoeff();
    zs[freq_index] = U_H[freq_index] * ps;
    /* STEP 2: Prepare and do solving */
    /* - Generate function: f(val) = h(val) - rho */
    Eigen::Matrix<double, -1, 1> inv_gamma; /* matrix gamma.(-1) (Note: not diag) */
    Eigen::Matrix<double, -1, 1> zs2;       /* zs2 = zs .* zs */
    inv_gamma = gamma[freq_index].array().pow(-1.0).matrix();
    zs2 = zs[freq_index].array().abs2().matrix();
    auto func_lambda = [&zs2, &inv_gamma, &rho](double val) -> double
    {
        Eigen::Matrix<double, -1, 1> part0 = (inv_gamma.array() + val).matrix();
        Eigen::Matrix<double, -1, 1> part1 = part0.array().pow(-1.0).matrix();
        Eigen::Matrix<double, -1, 1> part2 = part0.array().pow(-2.0).matrix();
        return zs2.dot(part2) / pow(zs2.dot(part1), 2.0) - rho;
    };
    /* - Generate range */
    vuprs::IterationConfig iter;
    vuprs::SetIterationConfigDefault(&iter);
    iter.lower_region = -(1.0 / max_eig);                                              /* -1/max(eig) */
    iter.upper_region = ((1.0 / min_eig) - sqrt_M_rho / max_eig) / (sqrt_M_rho - 1.0); /* [(1/min(eig) - sqrt(M * rho)/max(eig)) / (sqrt(M*rho) - 1)] */
    iter.func = func_lambda;
    /* - Iteration */
    double lambda; /* result lambda */
    BisectionIteration1D(iter, &lambda);
    /* STEP 3: Get ps_hat */
    inv_gamma_lambda_I[freq_index] = (inv_gamma.array() + lambda).array().pow(-1.0).matrix().asDiagonal();
    invR_lambdaI_inv_mul_ps[freq_index] = U[freq_index] * inv_gamma_lambda_I[freq_index] * U_H[freq_index] * ps;
    ps_hat[freq_index] = ((double)M - e0 / 2.0) * invR_lambdaI_inv_mul_ps[freq_index] / ((ps.adjoint() * invR_lambdaI_inv_mul_ps[freq_index])(0, 0));
    /* STEP 4: Get w */
    invR[freq_index] = U[freq_index] * inv_gamma.asDiagonal() * U_H[freq_index];
    invR_ps_hat[freq_index] = invR[freq_index] * ps_hat[freq_index];
    w[freq_index] = invR_ps_hat[freq_index] / (ps_hat[freq_index].adjoint() * invR_ps_hat[freq_index])(0, 0);
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
    double fs = this->signal_frequency_list(freq_index);
    int M = this->array.size();
    /* Compute */
    invR[freq_index] = R[freq_index].inverse();
    invR_ps[freq_index] = invR[freq_index] * ps[freq_index];
    w[freq_index] = invR_ps[freq_index] / (ps[freq_index].adjoint() * invR_ps[freq_index])(0, 0);
    /* Dump */
    this->result_weight_vectors.col(freq_index) = w[freq_index];
}
