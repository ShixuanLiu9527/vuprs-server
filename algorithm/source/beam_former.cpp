#include "beam_former.h"

/* ------------------------------------------------------------------------------ */
/* ---------------------------------- DCRCB ------------------------------------- */
/* ------------------------------------------------------------------------------ */

vuprs::Beamformer_DCRCB::Beamformer_DCRCB()
{
    
}

vuprs::Beamformer_DCRCB::~Beamformer_DCRCB()
{

}

void vuprs::Beamformer_DCRCB::CalculateBeamformingForOneFreq(int freqIndex)
{
    if (freqIndex == 0)
    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
        this->resultWeightVectors.col(freqIndex) *= 0;
        return;
    }

    Eigen::Matrix<Eigen::dcomplex, -1, 1> ps;  /* ps */
    Eigen::Matrix<Eigen::dcomplex, -1, -1> covMatrix;  /* cov matrix */
    double signalFreq;
    double steeringErrorRadius;
    int M;

    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
        ps = this->steeringVectors.col(freqIndex);  /* ps */
        signalFreq = this->signalFrequencyList(freqIndex);
        covMatrix = this->estimate_covMatrix[freqIndex];
        steeringErrorRadius = this->array.CalculateSteeringVectorErrorRadius(signalFreq);
        M = this->array.elementArray.size();
    }

    Eigen::Matrix<double, -1, 1> gamma_eigenvalues;  /* matrix gamma (pre diag) */
    Eigen::Matrix<double, -1, 1> inv_gamma_eigenvalues;  /* matrix gamma.(-1) (pre diag) */
    Eigen::Matrix<double, -1, 1> zs2;  /* zs .* zs */
    Eigen::Matrix<Eigen::dcomplex, -1, -1> u_eigenvectors;  /* matrix U, R = U * GAMMA * U.H */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> zs;  /* zs = U.H @ ps */

    vuprs::IterationConfig iter;

    double max_eigenvalues, min_eigenvalues;
    double _max_eigenvalues;  /* 1 / max(eig) */
    double _min_eigenvalues;  /* 1 / min(eig) */
    double rho = (double)M / pow((double)M - steeringErrorRadius / 2.0, 2.0);
    double sqrt_M_rho = sqrt((double)M * rho);
    double result_val;  /* result lambda */
    
    vuprs::SetIterationConfigDefault(&iter);

    /* Eigenvalue decomposition */

    vuprs::EigenvalueDecomposition(covMatrix, &gamma_eigenvalues, &u_eigenvectors);

    inv_gamma_eigenvalues = gamma_eigenvalues.array().pow(-1.0).matrix();

    min_eigenvalues = gamma_eigenvalues.minCoeff();
    max_eigenvalues = gamma_eigenvalues.maxCoeff();

    _min_eigenvalues = 1.0 / min_eigenvalues;
    _max_eigenvalues = 1.0 / max_eigenvalues;

    zs = u_eigenvectors.adjoint() * ps;
    zs2 = zs.array().abs2().matrix();

    /* Generate range */

    iter.lowerRegion = -_min_eigenvalues;
    iter.upperRegion = (_max_eigenvalues - sqrt_M_rho * _min_eigenvalues) / (sqrt_M_rho - 1.0);
    
    /* Generate function: f(val) = h(val) - rho */

    auto func_lambda = [&zs2, &inv_gamma_eigenvalues, &rho](double val) -> double
    {
        Eigen::Matrix<double, -1, 1> part0 = (inv_gamma_eigenvalues.array() + val).matrix();
        Eigen::Matrix<double, -1, 1> part1 = part0.array().pow(-1.0).matrix();
        Eigen::Matrix<double, -1, 1> part2 = part0.array().pow(-2.0).matrix();

        return zs2.dot(part2) / pow(zs2.dot(part1), 2.0) - rho;
    };

    iter.func = func_lambda;

    /* Iteration */

    BisectionIteration1D(iter, &result_val);

    /* Get weight vector for this frequency band */

    Eigen::Matrix<Eigen::dcomplex, -1, -1> invR_plus_lambdaI_inv = \
        u_eigenvectors * ((inv_gamma_eigenvalues.array() + result_val).array().pow(-1.0).matrix().asDiagonal()) * u_eigenvectors.adjoint();  /* (R.-1 + lambda * I).-1 */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> invR_plus_lambdaI_inv__mul__ps = invR_plus_lambdaI_inv * ps;

    Eigen::Matrix<Eigen::dcomplex, -1, 1> ps_estimate = \
        ((double)M - steeringErrorRadius / 2.0) * invR_plus_lambdaI_inv__mul__ps / (ps.adjoint() * invR_plus_lambdaI_inv__mul__ps)(0, 0);

    Eigen::Matrix<Eigen::dcomplex, -1, -1> invR = u_eigenvectors * inv_gamma_eigenvalues.asDiagonal() * u_eigenvectors.adjoint();  /* R.-1 = U * GAMMA.-1 * U.H */
    Eigen::Matrix<Eigen::dcomplex, -1, -1> invR__mul__ps_estimate = invR * ps_estimate;

    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
        this->resultWeightVectors.col(freqIndex) = invR__mul__ps_estimate / (ps_estimate.adjoint() * invR__mul__ps_estimate)(0, 0);
    }
    
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

void vuprs::Beamformer_CBF::CalculateBeamformingForOneFreq(int freqIndex)
{
    std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
    if (freqIndex == 0)
    {
        this->resultWeightVectors.col(freqIndex) *= 0;
        return;
    }
    Eigen::Matrix<Eigen::dcomplex, -1, 1> ps = this->steeringVectors.col(freqIndex);  /* ps */
    double M = this->array.elementArray.size();  /* M */
    this->resultWeightVectors.col(freqIndex) = ps / M;
}
