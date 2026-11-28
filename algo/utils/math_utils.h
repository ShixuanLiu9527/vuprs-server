#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <Eigen/Dense>
#include <complex>
#include <functional>
#include <vector>

namespace vuprs
{
    struct IterationConfig
    {
        std::function<double(double)> func; /* Original Function */
        std::function<double(double)> diff; /* First Derivative */
        double region_lo;                   /* lower boundary */
        double region_hi;                   /* upper boundary */
        double jumpout;                     /* jumpout threshold */
        int max_iteration;                  /* max iteration counts */
    };

    /** @brief Clamp value into [lo, hi]. */
    template <typename T>
    inline T Clamp(const T &value, const T &lo, const T &hi)
    {
        return value < lo ? lo : (value > hi ? hi : value);
    }

    double rad2deg(double rad);
    double deg2rad(double deg);

    /**
     * @brief Alt, Az to pointing vector.
     *
     * @param alt altitude in degrees.
     * @param az azimuth in degrees.
     */
    Eigen::Matrix<double, 3, 1> AltAz2PointingVector(double alt, double az);

    /**
     * @brief Pointing vector to Alt, Az.
     *
     * @param vec pointing vector.
     * @param alt output alt in degrees.
     * @param az output az in degrees.
     */
    void PointingVector2AltAz(const Eigen::Matrix<double, 3, 1> &vec, double *alt, double *az);

    /**
     * @brief Set IterationConfig obj to default value.
     */
    void SetIterationConfigDefault(IterationConfig *config);

    /**
     * @brief Secant iteration for 1D function.
     *
     * @note result = Solve(func(val) == 0), val in [region_lo, region_hi].
     *
     * @param config iteration configuration.
     * @param result output result.
     *
     * @throw std::runtime_error
     */
    bool SecantIteration1D(const vuprs::IterationConfig &config, double *result);

    /**
     * @brief Newton iteration for 1D function.
     *
     * @note result = Solve(func(val) == 0), val in [region_lo, region_hi].
     *
     * @param config iteration configuration.
     * @param result output result.
     *
     * @throw std::runtime_error
     */
    bool NewtonIteration1D(const vuprs::IterationConfig &config, double *result);

    /**
     * @brief Robust newton iteration for 1D function.
     *
     * @note result = Solve(func(val) == 0), val in [region_lo, region_hi].
     *
     * @param config iteration configuration.
     * @param result output result.
     *
     * @throw std::runtime_error
     */
    bool RobustNewtonIteration1D(const vuprs::IterationConfig &config, double *result);

    /**
     * @brief Bisection iteration for 1D function.
     *
     * @note result = Solve(func(val) == 0), val in [region_lo, region_hi].
     *
     * @param config iteration configuration.
     * @param result output result.
     *
     * @throw std::runtime_error
     */
    bool BisectionIteration1D(const vuprs::IterationConfig &config, double *result);

    /**
     * @brief Eigenvalue decomposition for covariance matrix.
     *
     * @param cov_matrix input covariance matrix.
     * @param eigenvalues output eigenvalues [eig1, eig2, ..., eigM].T
     * @param eigenvectors output eigenvectors [eigenvector1, eigenvector2, ..., eigenvectorM] (size = M x M).
     */
    void EigenvalueDecomposition(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &cov_matrix,
                                 Eigen::Matrix<double, -1, 1> *eigenvalues,
                                 Eigen::Matrix<Eigen::dcomplex, -1, -1> *eigenvectors);

    /**
     * @brief Eigenvalue decomposition for covariance matrix (external solver).
     *
     * @param cov_matrix input covariance matrix.
     * @param solver eigen decomposition solver.
     * @param eigenvalues output eigenvalues [eig1, eig2, ..., eigM].T
     * @param eigenvectors output eigenvectors [eigenvector1, eigenvector2, ..., eigenvectorM] (size = M x M).
     */
    void EigenvalueDecomposition(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &cov_matrix,
                                 Eigen::SelfAdjointEigenSolver<Eigen::MatrixXcd> &solver,
                                 Eigen::Matrix<double, -1, 1> *eigenvalues,
                                 Eigen::Matrix<Eigen::dcomplex, -1, -1> *eigenvectors);

    /**
     * @brief Cholesky decomposition for covariance matrix.
     *
     * @param R input covariance matrix.
     * @param G output lower triangular matrix G, where R = G * G.H
     */
    void CholeskyDecomposition(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &R,
                               Eigen::Matrix<Eigen::dcomplex, -1, -1> *G);

    /**
     * @brief Get FIR exp matrix (E).
     *
     * @note N: Signal points, L: FIR Filter Length.
     * @note E(k,l) = exp(-j * 2 * pi * k * l / N).
     *
     * @param L_fir FIR Filter length.
     * @param N_points signal points.
     * @param exp_matrix output matrix E.
     * @param use_positive_freq true: E.size = (N/2+1) x L, false: E.size = N x L.
     */
    void Get_FIR_EXPMatrix(int L_fir,
                           int N_points,
                           Eigen::Matrix<Eigen::dcomplex, -1, -1> *exp_matrix,
                           bool use_positive_freq);

    /**
     * @brief Get scan points using Fibonacci lattice.
     *
     * @note Only points that [alt > alt_min] are generated, and the distribution is uniform on the upper hemisphere.
     * @note Total points = 2 * n + 1 (n points in upper hemisphere, n points in lower hemisphere, and 1 point at the pole), but only points in upper hemisphere are returned.
     *
     * @param n_in_half number of scan points in each hemisphere (the point count in the half sphere).
     * @param alt_min minimum altitude for generated scan points (units: degrees).
     * @param alt output vector of altitudes (units: degrees).
     * @param az output vector of azimuths (units: degrees).
     */
    void FibonacciGrid(int n_in_half,
                       double alt_min,
                       std::vector<double> *alt,
                       std::vector<double> *az);

    double ConditionNumber(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &R);
}

#endif
