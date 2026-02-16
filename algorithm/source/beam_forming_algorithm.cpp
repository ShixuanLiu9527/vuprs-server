#include "beam_forming_algorithm.h"

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
    if (config.func == nullptr)
    {
        throw std::runtime_error("Original function = NULL.");
    }
    if (result == nullptr)
    {
        throw std::runtime_error("result = NULL.");
    }
    
    double f_lower = config.func(config.lowerRegion);
    double f_upper = config.func(config.upperRegion);
    if (f_lower * f_upper > 0.0)
    {
        throw std::runtime_error("No zeros within the specified range.");
    }
    
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
    if (config.func == nullptr)
    {
        throw std::runtime_error("Original function = NULL.");
    }
    if (config.func != nullptr && config.diff == nullptr)
    {
        return vuprs::BisectionIteration1D(config, result);
    }
    if (result == nullptr)
    {
        throw std::runtime_error("result = NULL.");
    }
    if (config.func(config.lowerRegion) * config.func(config.upperRegion) > 0.0)
    {
        throw std::runtime_error("No zeros within the specified range.");
    }

    double x = 0.5 * (config.lowerRegion + config.upperRegion), xp1 = x;
    double fx = 0.0, fxp1 = 0.0, diff_x = 0.0;
    int maxIteration = std::max(config.maxIteration, 10);
    bool thresholdJump = false;

    for (int i = 0; i < maxIteration; i++)
    {
        fx = config.func(x);
        diff_x = config.diff(x);

        if (abs(diff_x) < config.jumpout)
        {
            return vuprs::BisectionIteration1D(config, result);  /* Try bisection */
        }

        xp1 = x - fx / diff_x;
        fxp1 = config.func(xp1);

        if (abs(x - xp1) < config.jumpout && abs(fxp1) < config.jumpout) 
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
        return vuprs::RobustNewtonIteration1D(config, result);  /* Try robust algorithm */
    }

    return true;
}

bool vuprs::RobustNewtonIteration1D(const vuprs::IterationConfig &config, double *result)
{
    if (config.func == nullptr)
    {
        throw std::runtime_error("Original function = NULL.");
    }
    if (config.func != nullptr && config.diff == nullptr)
    {
        return vuprs::BisectionIteration1D(config, result);
    }
    if (result == nullptr)
    {
        throw std::runtime_error("result = NULL.");
    }
    if (config.func(config.lowerRegion) * config.func(config.upperRegion) > 0.0)
    {
        throw std::runtime_error("No zeros within the specified range.");
    }

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

        if (abs(diff_x) < config.jumpout)
        {
            return vuprs::BisectionIteration1D(config, result);  /* Try bisection */
        }

        increase = fx / diff_x;

        lambda = 1.0;

        for (int j = 0; j < maxIterationLambda; j++)
        {
            xp1 = x - lambda * increase;
            fxp1 = config.func(xp1);
            if (abs(fxp1) < abs(fx)) break;
            lambda = lambda * 0.5;
        }

        if (abs(x - xp1) < config.jumpout && abs(fxp1) < config.jumpout) 
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
        return vuprs::BisectionIteration1D(config, result);  /* Try bisection */
    }

    return true;
}

bool vuprs::BisectionIteration1D(const vuprs::IterationConfig &config, double *result)
{
    if (config.func == nullptr)
    {
        throw std::runtime_error("Original function = NULL.");
    }
    if (result == nullptr)
    {
        throw std::runtime_error("result = NULL.");
    }

    double left = config.lowerRegion, right = config.upperRegion, mid = 0.5 * (left + right);
    double fl = config.func(left), fr = config.func(right), fm = 0.0;
    int maxIteration = std::max(config.maxIteration, 10);
    
    if (fl * fr > 0.0)
    {
        if (abs(fl) < config.jumpout)
        {
            *result = left;
            return true;
        }
        if (abs(fr) < config.jumpout)
        {
            *result = right;
            return true;
        }
        throw std::runtime_error("No zeros within the specified range.");
    }

    *result = mid;

    for (int i = 0; i < maxIteration; i++)
    {
        mid = 0.5 * (left + right);
        fm = config.func(mid);

        if (abs(fm) < config.jumpout)
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

        if (abs(left - right) < config.jumpout)
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

void vuprs::EigenvalueDecomposition(
    const Eigen::Matrix<Eigen::dcomplex, -1, -1> &covMatrix, 
    Eigen::Matrix<double, -1, 1> *eigenvalues, 
    Eigen::Matrix<Eigen::dcomplex, -1, -1> *eigenvectors)
{
    if (covMatrix.rows() != covMatrix.cols()) 
    {
        throw std::invalid_argument("Covariance matrix must be square");
    }
    
    if (eigenvalues == nullptr || eigenvectors == nullptr) 
    {
        throw std::invalid_argument("Output pointers cannot be null");
    }
    
    Eigen::MatrixXcd H = (covMatrix + covMatrix.adjoint()) / 2.0;
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXcd> solver;
    
    solver.compute(H);
    
    if (solver.info() != Eigen::Success) 
    {
        throw std::runtime_error("Failed to solving eigenvalues.");
    }
    
    *eigenvalues = solver.eigenvalues().real();
    *eigenvectors = solver.eigenvectors();
}

void vuprs::GetFrequencyResponseFIR_OneChannel(const Eigen::Matrix<Eigen::dcomplex, -1, 1> &frequencyResponseHf, double fs, Eigen::Matrix<double, -1, 1> *h)
{
    int size = frequencyResponseHf.rows();

    Eigen::Matrix<Eigen::dcomplex, -1, 1> _frequencyResponseHf = frequencyResponseHf;
    Eigen::Matrix<Eigen::dcomplex, -1, 1> _h;
    
    vuprs::SignalMontage(&_frequencyResponseHf);  /* signal montage */
    vuprs::FFT(_frequencyResponseHf, &_h, true);

    *h = _h.real();
}
