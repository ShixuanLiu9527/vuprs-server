#include "algorithm/bf/beam_forming_algorithm.h"
#include "logger/check.h"

#define BEAM_FORMING_ALGORITHM_DEBUG_PRINT false

void vuprs::SetIterationConfigDefault(IterationConfig *config)
{
    config->func = nullptr;
    config->diff = nullptr;
    config->lower_region = 0.0;
    config->upper_region = 0.0;
    config->jumpout = 1e-10;
    config->max_iteration = 2000;
}

bool vuprs::SecantIteration1D(const vuprs::IterationConfig &config, double *result)
{
    RUNTIME_CHECK(config.func != nullptr, "bf", "Original function = NULL");
    RUNTIME_CHECK(result != nullptr, "bf", "Original result = NULL");

    double f_lower = config.func(config.lower_region);
    double f_upper = config.func(config.upper_region);

    RUNTIME_CHECK(f_lower * f_upper <= 0.0, "bf", "No zeros within the specified range");
    double x0 = config.lower_region;
    double x1 = config.upper_region;
    double f0 = f_lower;
    double f1 = f_upper;
    int max_iter = std::max(config.max_iteration, 30);
    double tol = config.jumpout;
    for (int i = 0; i < max_iter; i++)
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
        if (x2 < config.lower_region || x2 > config.upper_region)
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
    RUNTIME_CHECK(config.func(config.lower_region) * config.func(config.upper_region) <= 0.0,
                  "bf",
                  "No zeros within the specified range");

    double x = 0.5 * (config.lower_region + config.upper_region), xp1 = x;
    double fx = 0.0, fxp1 = 0.0, diff_x = 0.0;
    int max_iteration = std::max(config.max_iteration, 10);
    bool threshold_jump = false;
    for (int i = 0; i < max_iteration; i++)
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
            threshold_jump = true;
            break;
        }
        if (xp1 > config.upper_region || xp1 < config.lower_region)
        {
            break;
        }
        x = xp1;
    }
    *result = xp1;
    if (!threshold_jump)
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
    RUNTIME_CHECK(config.func(config.lower_region) * config.func(config.upper_region) <= 0.0,
                  "bf",
                  "No zeros within the specified range");

    double x = 0.5 * (config.lower_region + config.upper_region), xp1 = x, increase = 0.0;
    double fx = config.func(x), fxp1 = 0.0, diff_x;
    double lambda = 1.0;
    int max_iteration = std::max(config.max_iteration, 10),
        max_iteration_lambda = std::max(config.max_iteration / 10, 10);
    bool threshold_jump = false;
    for (int i = 0; i < max_iteration; i++)
    {
        fx = config.func(x);
        diff_x = config.diff(x);
        if (std::abs(diff_x) < config.jumpout)
        {
            return vuprs::BisectionIteration1D(config, result); /* Try bisection */
        }
        increase = fx / diff_x;
        lambda = 1.0;
        for (int j = 0; j < max_iteration_lambda; j++)
        {
            xp1 = x - lambda * increase;
            fxp1 = config.func(xp1);
            if (std::abs(fxp1) < std::abs(fx))
                break;
            lambda = lambda * 0.5;
        }
        if (std::abs(x - xp1) < config.jumpout && std::abs(fxp1) < config.jumpout)
        {
            threshold_jump = true;
            break;
        }
        if (xp1 > config.upper_region || xp1 < config.lower_region)
        {
            break;
        }
        x = xp1;
    }
    *result = xp1;
    if (!threshold_jump)
    {
        return vuprs::BisectionIteration1D(config, result); /* Try bisection */
    }
    return true;
}

bool vuprs::BisectionIteration1D(const vuprs::IterationConfig &config, double *result)
{
    RUNTIME_CHECK(config.func != nullptr, "bf", "Original function = NULL");
    RUNTIME_CHECK(result != nullptr, "bf", "result = NULL");

    double left = config.lower_region, right = config.upper_region, mid = 0.5 * (left + right);
    double fl = config.func(left), fr = config.func(right), fm = 0.0;
    int max_iteration = std::max(config.max_iteration, 10);
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
    for (int i = 0; i < max_iteration; i++)
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
        if (mid > config.upper_region || mid < config.lower_region)
        {
            break;
        }
    }
    return false;
}

void vuprs::EigenvalueDecomposition(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &cov_matrix,
                                    Eigen::Matrix<double, -1, 1> *eigenvalues,
                                    Eigen::Matrix<Eigen::dcomplex, -1, -1> *eigenvectors)
{
    RUNTIME_CHECK(cov_matrix.rows() == cov_matrix.cols(), "bf", "Covariance matrix must be square");
    RUNTIME_CHECK(eigenvalues && eigenvectors, "bf", "Output pointers cannot be null");
    Eigen::MatrixXcd H = (cov_matrix + cov_matrix.adjoint()) / 2.0;
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXcd> solver;
    solver.compute(H);
    RUNTIME_CHECK(solver.info() == Eigen::Success, "bf", "Failed to solving eigenvalues");
    *eigenvalues = solver.eigenvalues().real();
    *eigenvectors = solver.eigenvectors();
}

void vuprs::CholeskyDecomposition(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &cov_matrix,
                                  Eigen::Matrix<Eigen::dcomplex, -1, -1> *G)
{
    RUNTIME_CHECK(cov_matrix.rows() == cov_matrix.cols(), "bf", "Covariance matrix must be square");
    RUNTIME_CHECK(G, "bf", "Output pointer cannot be null");
    Eigen::Matrix<double, -1, 1> gamma;
    Eigen::Matrix<Eigen::dcomplex, -1, -1> U; /* R = U * gamma * U.H */
    vuprs::EigenvalueDecomposition(cov_matrix, &gamma, &U);
    *G = U * gamma.cwiseSqrt().asDiagonal(); /* R = U * gamma * U.H = B * B.H */
}

void vuprs::Get_FIR_EXPMatrix(int L_fir, int N_points, Eigen::Matrix<Eigen::dcomplex, -1, -1> *exp_matrix, bool use_positive_freq)
{
    int output_size;
    if (use_positive_freq)
        output_size = N_points / 2 + 1;
    else
        output_size = N_points;
    Eigen::Matrix<double, -1, 1> l_vec = Eigen::Matrix<double, -1, 1>::LinSpaced(L_fir, 0, L_fir - 1);
    Eigen::Matrix<double, -1, 1> k_vec = Eigen::Matrix<double, -1, 1>::LinSpaced(output_size, 0, output_size - 1);
    exp_matrix->resize(output_size, L_fir);
    exp_matrix->setOnes();                                                      /* 1 */
    *exp_matrix *= -2.0 * PI * std::complex<double>(0, 1.0) / (double)N_points; /* -j * 2 * pi / N */
    *exp_matrix = exp_matrix->array() * (k_vec * l_vec.transpose()).array();    /* -j * 2 * pi * k * l / N */
    *exp_matrix = exp_matrix->array().exp().matrix();                           /* E(k,l) = exp(-j * 2 * pi * k * l / N) */
}

void vuprs::FibonacciGrid(int n_in_half,
                          double alt_min,
                          std::vector<double> *alt,
                          std::vector<double> *az)
{
    RUNTIME_CHECK(alt && az, "bf", "Output pointers cannot be null");
    double phi = (std::sqrt(5.0) - 1.0) / 2.0; /* Golden ratio */
    double xn, yn, zn, _alt, _az;
    int N = 2 * n_in_half + 1, n;
    int start_n = (N + 1) / 2; /* Number of points in upper hemisphere */
    int result_size = N - start_n + 1;
    Eigen::Matrix<double, 3, 1> vec;
    alt->clear();
    az->clear();
    alt->reserve(result_size);
    az->reserve(result_size);
    for (int i = 0; i < result_size; i++)
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
