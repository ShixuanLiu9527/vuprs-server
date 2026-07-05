#ifndef BEAM_FORMING_ALGORITHM_H
#define BEAM_FORMING_ALGORITHM_H

#include <Eigen/Dense>
#include <vector>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>

#include "signal_processing.h"

namespace vuprs
{
    struct IterationConfig
    {
        std::function<double(double)> func;  /* Original Function */
        std::function<double(double)> diff;  /* First Derivative */
        double lowerRegion;  /* lower boundary */
        double upperRegion;  /* upper boundary */
        double jumpout;  /* jumpout threshold */
        int maxIteration;  /* max iteration counts */
    };

    class ThreadPool 
    {
        private:

            std::vector<std::thread> workers;
            std::queue<std::function<void()>> tasks;
            std::mutex queue_mutex;
            std::condition_variable condition;
            bool stop;

        public:
            ThreadPool(size_t numThreads) : stop(false) 
            {
                for (size_t i = 0; i < numThreads; ++i) 
                {
                    /* Create one thread */

                    workers.emplace_back([this] 
                    {
                        while (true)
                        {
                            std::function<void()> task;

                            {
                                std::unique_lock<std::mutex> lock(this->queue_mutex);

                                this->condition.wait(lock, [this] {return this->stop || !this->tasks.empty();});

                                if (this->stop && this->tasks.empty()) return;

                                task = std::move(this->tasks.front());

                                this->tasks.pop();
                            }

                            try 
                            {
                                task();  /* Do task */
                            } 
                            catch (const std::exception& e) 
                            {
                                std::cout << "Task threw exception: " << e.what() << std::endl;
                            } 
                            catch (...)
                            {
                                std::cout << "Task threw unknown exception." << std::endl;
                            }
                        }
                    });
                }
            }
    
            template<class F, class... Args>
            std::future<typename std::result_of<F(Args...)>::type> enqueue(F&& f, Args&&... args) 
            {
                using return_type = typename std::result_of<F(Args...)>::type;
        
                auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        
                std::future<return_type> result = task->get_future();

                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    if (this->stop) throw std::runtime_error("Enqueue on stopped ThreadPool");
                    tasks.emplace([task](){ (*task)(); });
                }

                condition.notify_one();
                return result;
            }
    
            ~ThreadPool() 
            {
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    stop = true;
                }
                condition.notify_all();
                for (std::thread &worker : workers) 
                {
                    worker.join();
                }
            }
    };

    inline double rad2deg(double rad) {return rad / PI * 180.0;}
    inline double deg2rad(double deg) {return deg / 180.0 * PI;}

    /**
     * @brief Alt, Az to pointing vector.
     * 
     * @param alt altitude in degrees.
     * @param az azimuth in degrees.
     */
    inline Eigen::Matrix<double, 3, 1> AltAz2PointingVector(double alt, double az)
    {
        Eigen::Matrix<double, 3, 1> vec;
        double c_alt = cos(vuprs::deg2rad(alt)), c_az = cos(vuprs::deg2rad(az)), \
               s_alt = sin(vuprs::deg2rad(alt)), s_az = sin(vuprs::deg2rad(az));
        vec << c_alt * s_az, c_alt * c_az, s_alt;
        return vec.normalized();
    }

    /**
     * @brief Pointing vector to Alt, Az.
     * 
     * @param vec pointing vector.
     * @param alt output alt in degrees.
     * @param az output az in degrees.
     */
    inline void PointingVector2AltAz(const Eigen::Matrix<double, 3, 1> &vec, double *alt, double *az)
    {
        double x = vec(0, 0), y = vec(1, 0), z = vec(2, 0);
        if (z > 1.0) z = 1.0;
        else if (z < -1.0) z = -1.0;
        *alt = vuprs::rad2deg(asin(z));
        *az = vuprs::rad2deg(atan2(x, y));
    }

    /**
     * @brief Set IterationConfig obj to default value.
     */
    void SetIterationConfigDefault(IterationConfig *config);

    /**
     * @brief Secant iteration for 1D function.
     * 
     * @note result = Solve(func(val) == 0), val in [lowerRegion, upperRegion].
     * 
     * @param config iteration configuration.
     * @param result output result.
     * 
     * @throw std::runtime_error
     */
    bool SecantIteration1D(const vuprs::IterationConfig &config, double *result);

    /**
     * @brief Robust secant iteration for 1D function.
     * 
     * @note result = Solve(func(val) == 0), val in [lowerRegion, upperRegion].
     * 
     * @param config iteration configuration.
     * @param result output result.
     * 
     * @throw std::runtime_error
     */
    bool RobustSecantIteration1D(const vuprs::IterationConfig &config, double *result);

    /**
     * @brief Newton iteration for 1D function.
     * 
     * @note result = Solve(func(val) == 0), val in [lowerRegion, upperRegion].
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
     * @note result = Solve(func(val) == 0), val in [lowerRegion, upperRegion].
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
     * @note result = Solve(func(val) == 0), val in [lowerRegion, upperRegion].
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
     * @param covMatrix input covariance matrix.
     * @param eigenvalues output eigenvalues [eig1, eig2, ..., eigM].T
     * @param eigenvectors output eigenvectors [eigenvector1, eigenvector2, ..., eigenvectorM] (size = M x M).
     */
    void EigenvalueDecomposition(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &covMatrix, 
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
     * @param expMatrix output matrix E.
     * @param usePositiveFreq true: E.size = (N/2+1) x L, false: E.size = N x L.
     */
    void Get_FIR_EXPMatrix(int L_fir, int N_points, Eigen::Matrix<Eigen::dcomplex, -1, -1> *expMatrix, bool usePositiveFreq);

    /**
     * @brief Get scan points using Fibonacci lattice.
     * 
     * @note Only points that [alt > alt_min] are generated, and the distribution is uniform on the upper hemisphere.
     * @note Total points = 2 * n + 1 (n points in upper hemisphere, n points in lower hemisphere, and 1 point at the pole), but only points in upper hemisphere are returned.
     * 
     * @param nInHalf number of scan points in each hemisphere (the point count in the half sphere).
     * @param alt output vector of altitudes (units: degrees).
     * @param az output vector of azimuths (units: degrees).
     * @param alt_min minimum altitude for generated scan points (units: degrees).

     */
    void FibonacciGrid(int nInHalf, std::vector<double> *alt, std::vector<double> *az, double alt_min);

    double ConditionNumber(const Eigen::Matrix<Eigen::dcomplex, -1, -1> &R);
}

#endif
