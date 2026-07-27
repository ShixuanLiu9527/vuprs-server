#include "algorithm/bf/beam_forming_algorithm.h"
#include "logger/log_manager.h"

#define BEAM_FORMING_ALGORITHM_DEBUG_PRINT false

void vuprs::SetIterationConfigDefault(IterationConfig *config)
{
    config->func = nullptr;
    config->diff = nullptr;
    config->lowerRegion = 0.0;
    config->upperRegion = 0.0;
    config->jumpout = 1e-10;
    config->maxIteration = 2000;
}

bool vuprs::SecantIteration1D(const vuprs::IterationConfig &config, double *result)
{
    RUNTIME_CHECK(config.func != nullptr, "bf", "Original function = NULL");
    RUNTIME_CHECK(result != nullptr, "bf", "Original result = NULL");

    double f_lower = config.func(config.lowerRegion);
    double f_upper = config.func(config.upperRegion);

    RUNTIME_CHECK(f_lower * f_upper <= 0.0, "bf", "No zeros within the specified range");
    double x0 = config.lowerRegion;
    double x1 = config.upperRegion;
    double f0 = f_lower;
    double f1 = f_upper;
    int maxIter = std::max(config.maxIteration, 30);
    double tol = config.jumpout;
    for (int i = 0; i < maxIter; i++)
    {
        if (std::abs(f1 - f0) < 1e-15)
        {
            double x_mid = 0.5 * (x0 + x1);
            double f_mid = config.func(x_mid);

            if (f0 * f_mid <= 0)
            {
                x1 = x_mid;
                f1 = f_mid;
            }
            else
            {
                x0 = x_mid;
                f0 = f_mid;
            }
            continue;
        }

        double x2 = x1 - f1 * (x1 - x0) / (f1 - f0);
        if (x2 < config.lowerRegion || x2 > config.upperRegion)
        {
            x2 = 0.5 * (x0 + x1);
        }
        double f2 = config.func(x2);
        if (std::abs(f2) < tol && std::abs(x2 - x1) < tol)
        {
            *result = x2;
            return true;
        }
        if (f0 * f2 <= 0)
        {
            x1 = x2;
            f1 = f2;
        }
        else
        {
            x0 = x2;
            f0 = f2;
        }
    }
    *result = 0.5 * (x0 + x1);
    return false;
}

bool vuprs::RobustSecantIteration1D(const vuprs::IterationConfig &config, double *result)
{
}

bool vuprs::NewtonIteration1D(const vuprs::IterationConfig &config, double *result)
{
    RUNTIME_CHECK(config.func != nullptr, "bf", "Original function = NULL");
    if (config.func != nullptr && config.diff == nullptr)
    {
        return vuprs::BisectionIteration1D(config, result);
    }
    RUNTIME_CHECK(result != nullptr, "bf", "result = NULL");
    RUNTIME_CHECK(config.func(config.lowerRegion) * config.func(config.upperRegion) <= 0.0,
                  "bf",
                  "No zeros within the specified range");

    double x = 0.5 * (config.lowerRegion + config.upperRegion), xp1 = x;
    double fx = 0.0, fxp1 = 0.0, diff_x = 0.0;
    int maxIteration = std::max(config.maxIteration, 10);
    bool thresholdJump = false;
    for (int i = 0; i < maxIteration; i++)
    {
        fx = config.func(x);
        diff_x = config.diff(x);
        if (std::abs(diff_x) < config.jumpout)
        {
            return vuprs::BisectionIteration1D(config, result); /* Try bisection */
        }
        xp1 = x - fx / diff_x;
        fxp1 = config.func(xp1);
        if (std::abs(x - xp1) < config.jumpout && std::abs(fxp1) < config.jumpout)
        {
            thresholdJump = true;
            break;
        }
        if (xp1 > config.upperRegion || xp1 < config.lowerRegion)
        {
            break;
        }
        x = xp1;
    }
    *result = xp1;
    if (!thresholdJump)
    {
        return vuprs::RobustNewtonIteration1D(config, result); /* Try robust algorithm */
    }
    return true;
}

bool vuprs::RobustNewtonIteration1D(const vuprs::IterationConfig &config, double *result)
{
    RUNTIME_CHECK(config.func != nullptr, "bf", "Original function = NULL");
    if (config.func != nullptr && config.diff == nullptr)
    {
        return vuprs::BisectionIteration1D(config, result);
    }
    RUNTIME_CHECK(result != nullptr, "bf", "result = NULL");
    RUNTIME_CHECK(config.func(config.lowerRegion) * config.func(config.upperRegion) <= 0.0,
                  "bf",
                  "No zeros within the specified range");

    double x = 0.5 * (config.lowerRegion + config.upperRegion), xp1 = x, increase = 0.0;
    double fx = config.func(x), fxp1 = 0.0, diff_x;
    double lambda = 1.0;
    int maxIteration = std::max(config.maxIteration, 10),
        maxIterationLambda = std::max(config.maxIteration / 10, 10);
    bool thresholdJump = false;
    for (int i = 0; i < maxIteration; i++)
    {
        fx = config.func(x);
        diff_x = config.diff(x);
        if (std::abs(diff_x) < config.jumpout)
        {
            return vuprs::BisectionIteration1D(config, result); /* Try bisection */
        }
        increase = fx / diff_x;
        lambda = 1.0;
        for (int j = 0; j < maxIterationLambda; j++)
        {
            xp1 = x - lambda * increase;
            fxp1 = config.func(xp1);
            if (std::abs(fxp1) < std::abs(fx))
                break;
            lambda = lambda * 0.5;
        }
        if (std::abs(x - xp1) < config.jumpout && std::abs(fxp1) < config.jumpout)
        {
            thresholdJump = true;
            break;
        }
        if (xp1 > config.upperRegion || xp1 < config.lowerRegion)
        {
            break;
        }
        x = xp1;
    }
    *result = xp1;
    if (!thresholdJump)
    {
        return vuprs::BisectionIteration1D(config, result); /* Try bisection */
    }
    return true;
}

bool vuprs::BisectionIteration1D(const vuprs::IterationConfig &config, double *result)
{
    RUNTIME_CHECK(config.func != nullptr, "bf", "Original function = NULL");
    RUNTIME_CHECK(result != nullptr, "bf", "result = NULL");

    double left = config.lowerRegion, right = config.upperRegion, mid = 0.5 * (left + right);
    double fl = config.func(left), fr = config.func(right), fm = 0.0;
    int maxIteration = std::max(config.maxIteration, 10);
    if (fl * fr > 0.0)
    {
        if (std::abs(fl) < config.jumpout)
        {
            *result = left;
            return true;
        }
        if (std::abs(fr) < config.jumpout)
        {
            *result = right;
            return true;
        }
        RUNTIME_CHECK(false, "bf", " in [vuprs::BisectionIteration1D] No zeros within the specified range.");
    }
    *result = mid;
    for (int i = 0; i < maxIteration; i++)
    {
        mid = 0.5 * (left + right);
        fm = config.func(mid);
        if (std::abs(fm) < config.jumpout)
        {
            *result = mid;
            return true;
        }
        if (fl * fm < 0.0)
        {
            right = mid;
            fr = fm;
        }
        else if (fm * fr < 0.0)
        {
            left = mid;
            fl = fm;
        }
        else
        {
            break;
        }
        if (std::abs(left - right) < config.jumpout)
        {
            *result = mid;
            return true;
        }
        if (mid > config.upperRegion || mid < config.lowerRegion)
        {
            break;
        }
    }
    return false;
}

void vuprs::EigenvalueDecomposition(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &covMatrix,
                                    Eigen::Matrix<double, -1, 1> *eigenvalues,
                                    Eigen::Matrix<Eigen::dcomplex, -1, -1> *eigenvectors)
{
    RUNTIME_CHECK(covMatrix.rows() == covMatrix.cols(), "bf", "Covariance matrix must be square");
    RUNTIME_CHECK(eigenvalues && eigenvectors, "bf", "Output pointers cannot be null");
    Eigen::MatrixXcd H = (covMatrix + covMatrix.adjoint()) / 2.0;
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXcd> solver;
    solver.compute(H);
    RUNTIME_CHECK(solver.info() == Eigen::Success, "bf", "Failed to solving eigenvalues");
    *eigenvalues = solver.eigenvalues().real();
    *eigenvectors = solver.eigenvectors();
}

void vuprs::CholeskyDecomposition(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &covMatrix,
                                  Eigen::Matrix<Eigen::dcomplex, -1, -1> *G)
{
    RUNTIME_CHECK(covMatrix.rows() == covMatrix.cols(), "bf", "Covariance matrix must be square");
    RUNTIME_CHECK(G, "bf", "Output pointer cannot be null");
    Eigen::Matrix<double, -1, 1> gamma;
    Eigen::Matrix<Eigen::dcomplex, -1, -1> U; /* R = U * gamma * U.H */
    vuprs::EigenvalueDecomposition(covMatrix, &gamma, &U);
    *G = U * gamma.cwiseSqrt().asDiagonal(); /* R = U * gamma * U.H = B * B.H */
}

void vuprs::Get_FIR_EXPMatrix(int L_fir, int N_points, Eigen::Matrix<Eigen::dcomplex, -1, -1> *expMatrix, bool usePositiveFreq)
{
    int outputSize;

    if (usePositiveFreq)
        outputSize = N_points / 2 + 1;
    else
        outputSize = N_points;
    Eigen::Matrix<double, -1, 1> l_vec = Eigen::Matrix<double, -1, 1>::LinSpaced(L_fir, 0, L_fir - 1);
    Eigen::Matrix<double, -1, 1> k_vec = Eigen::Matrix<double, -1, 1>::LinSpaced(outputSize, 0, outputSize - 1);
    expMatrix->resize(outputSize, L_fir);
    expMatrix->setOnes();                                                      /* 1 */
    *expMatrix *= -2.0 * PI * std::complex<double>(0, 1.0) / (double)N_points; /* -j * 2 * pi / N */
    *expMatrix = expMatrix->array() * (k_vec * l_vec.transpose()).array();     /* -j * 2 * pi * k * l / N */
    *expMatrix = expMatrix->array().exp().matrix();                            /* E(k,l) = exp(-j * 2 * pi * k * l / N) */
}

void vuprs::FibonacciGrid(int nInHalf, std::vector<double> *alt, std::vector<double> *az, double alt_min = 15.0)
{
    RUNTIME_CHECK(alt && az, "bf", "Output pointers cannot be null");

    double phi = (std::sqrt(5.0) - 1.0) / 2.0; /* Golden ratio */
    double xn, yn, zn, _alt, _az;
    int N = 2 * nInHalf + 1, n;
    int start_n = (N + 1) / 2; /* Number of points in upper hemisphere */
    int resultSize = N - start_n + 1;
    Eigen::Matrix<double, 3, 1> vec;
    alt->clear();
    az->clear();
    alt->reserve(resultSize);
    az->reserve(resultSize);
    for (int i = 0; i < resultSize; i++)
    {
        n = start_n + i;
        zn = 2.0 * (double)n / (double)N - 1.0;
        xn = std::sqrt(1.0 - zn * zn) * std::cos(2.0 * PI * (double)n * phi);
        yn = std::sqrt(1.0 - zn * zn) * std::sin(2.0 * PI * (double)n * phi);
        vec << xn, yn, zn;
        vuprs::PointingVector2AltAz(vec, &_alt, &_az);
        if (_alt < alt_min)
            continue;
        alt->push_back(_alt);
        az->push_back(_az);
    }
}

double vuprs::ConditionNumber(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &R)
{
    Eigen::JacobiSVD<Eigen::Matrix<Eigen::dcomplex, -1, -1>> svd(R);
    return svd.singularValues()(0) / svd.singularValues()(svd.singularValues().size() - 1);
}
