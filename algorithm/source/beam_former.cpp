#include "beam_former.h"

#define BEAM_FORMER_CPP__DEBUG_PRINT false
#define BEAM_FORMER_CPP__DEBUG_SAVE false

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

    Eigen::Matrix<Eigen::dcomplex, -1, 1> ps;  /* steering vector for this frequency: ps */
    Eigen::Matrix<Eigen::dcomplex, -1, -1> R;  /* covariance matrix: R */
    double fs;  /* sampling frequency: fs */
    double e0;  /* max steering error radius: e0 */
    int M;  /* element number: M */

    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
        ps = this->steeringVectors.col(freqIndex);
        fs = this->signalFrequencyList(freqIndex);
        R = this->estimate_covMatrix[freqIndex];
        e0 = this->array.CalculateSteeringVectorErrorRadius(fs);
        M = this->array.elementArray.size();
    }

    Eigen::Matrix<double, -1, 1> gamma;  /* matrix gamma (Note: not diag) */
    Eigen::Matrix<double, -1, 1> inv_gamma;  /* matrix gamma.(-1) (Note: not diag) */
    Eigen::Matrix<Eigen::dcomplex, -1, -1> U;  /* matrix U, R = U * gamma * U.H */
    Eigen::Matrix<Eigen::dcomplex, -1, -1> U_H;  /* matrix U.H, R = U * gamma * U.H */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> zs;  /* zs = U.H @ ps */
    Eigen::Matrix<double, -1, 1> zs2;  /* zs2 = zs .* zs */

    double max_eig;  /* max(eigenvalues) */
    double min_eig;  /* min(eigenvalues) */
    double rho = (double)M / pow((double)M - e0 / 2.0, 2.0);  /* M / (M - e0 / 2)^2 */
    double sqrt_M_rho = sqrt((double)M * rho);  /* sqrt(M * rho) */

    /* STEP 1: Eigenvalue decomposition for covariance matrix R */

    vuprs::EigenvalueDecomposition(R, &gamma, &U);

    inv_gamma = gamma.array().pow(-1.0).matrix();
    U_H = U.adjoint();

    min_eig = gamma.minCoeff();
    max_eig = gamma.maxCoeff();

    #if (BEAM_FORMER_CPP__DEBUG_SAVE || BEAM_FORMER_CPP__DEBUG_PRINT)

        /* DEBUG ! */

        if (freqIndex == 100 || freqIndex == 200)
        {
            #if BEAM_FORMER_CPP__DEBUG_PRINT
                printf("[debug] DCRCB: (max, min) eigenvalue (@ index = %d) is (%.6f, %.6f)\n", freqIndex, max_eig, min_eig);
            #endif
        }

    #endif

    zs = U_H * ps;
    zs2 = zs.array().abs2().matrix();

    /* STEP 2: Prepare and do solving */

    vuprs::IterationConfig iter;
    double lambda;  /* result lambda */
    
    vuprs::SetIterationConfigDefault(&iter);

    /* Generate range */

    iter.lowerRegion = -(1.0 / max_eig);  /* -1/max(eig) */
    iter.upperRegion = ((1.0 / min_eig) - sqrt_M_rho / max_eig) / (sqrt_M_rho - 1.0);  /* [(1/min(eig) - sqrt(M * rho)/max(eig)) / (sqrt(M*rho) - 1)] */
    
    /* Generate function: f(val) = h(val) - rho */

    auto func_lambda = [&zs2, &inv_gamma, &rho](double val) -> double
    {
        Eigen::Matrix<double, -1, 1> part0 = (inv_gamma.array() + val).matrix();
        Eigen::Matrix<double, -1, 1> part1 = part0.array().pow(-1.0).matrix();
        Eigen::Matrix<double, -1, 1> part2 = part0.array().pow(-2.0).matrix();

        return zs2.dot(part2) / pow(zs2.dot(part1), 2.0) - rho;
    };

    iter.func = func_lambda;

    /* Iteration */

    BisectionIteration1D(iter, &lambda);

    /* STEP 3: Get ps_hat */

    Eigen::Matrix<Eigen::dcomplex, -1, -1> inv_gamma_lambda_I;  /* (gamma.-1 + lambda * I).-1 */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> invR_lambdaI_inv_mul_ps;  /* ((R.-1 + lambda * I).-1) * ps */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> ps_hat;  /* (M - e0 / 2) * [((R.-1 + lambda * I).-1) * ps] / [ps.H * ((R.-1 + lambda * I).-1) * ps] */

    inv_gamma_lambda_I = (inv_gamma.array() + lambda).array().pow(-1.0).matrix().asDiagonal();
    invR_lambdaI_inv_mul_ps = U * inv_gamma_lambda_I * U_H * ps;
    ps_hat = ((double)M - e0 / 2.0) * invR_lambdaI_inv_mul_ps / ((ps.adjoint() * invR_lambdaI_inv_mul_ps)(0, 0));

    /* STEP 4: Get w */

    Eigen::Matrix<Eigen::dcomplex, -1, -1> invR;  /* R.-1 = U * GAMMA.-1 * U.H */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> invR_ps_hat;  /* R.-1 * ps.hat */
    Eigen::Matrix<Eigen::dcomplex, -1, 1> w;  /* result weight = (R.-1 * ps_hat) / (ps_hat.H * R.-1 * ps_hat) */

    invR = U * inv_gamma.asDiagonal() * U_H;
    invR_ps_hat = invR * ps_hat;
    w = invR_ps_hat / (ps_hat.adjoint() * invR_ps_hat)(0, 0);

    {
        std::unique_lock<std::mutex> lock(this->mut);  /* LOCK */
        this->resultWeightVectors.col(freqIndex) = w;

        #if (BEAM_FORMER_CPP__DEBUG_SAVE || BEAM_FORMER_CPP__DEBUG_PRINT)

            /* DEBUG ! */

            if (freqIndex == 100 || freqIndex == 200)
            {
                #if BEAM_FORMER_CPP__DEBUG_SAVE
                    vuprs::SaveToCSV_complex(this->resultWeightVectors.col(freqIndex), "../weights/weight_" + std::to_string(freqIndex) + ".csv");
                #endif
                #if BEAM_FORMER_CPP__DEBUG_PRINT
                    printf("[debug] DCRCB: frequency (@ index = %d) = %.6f Hz\n", freqIndex, this->signalFrequencyList(freqIndex));
                #endif
            }

        #endif
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

    #if (BEAM_FORMER_CPP__DEBUG_SAVE || BEAM_FORMER_CPP__DEBUG_PRINT)

        /* DEBUG ! */

        if (freqIndex == 100 || freqIndex == 200)
        {
            #if BEAM_FORMER_CPP__DEBUG_SAVE
                vuprs::SaveToCSV_complex(this->resultWeightVectors.col(freqIndex), "../weights/weight_" + std::to_string(freqIndex) + ".csv");
            #endif
            #if BEAM_FORMER_CPP__DEBUG_PRINT
                printf("[debug] CBF: frequency (@ index = %d) = %.6f Hz\n", freqIndex, this->signalFrequencyList(freqIndex));
            #endif
        }

    #endif
}
