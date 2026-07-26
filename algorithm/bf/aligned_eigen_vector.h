#ifndef ALIGNED_EIGEN_VECTOR
#define ALIGNED_EIGEN_VECTOR

#include <Eigen/Dense>
#include <vector>

namespace vuprs
{
    /**
     * @brief VUPRS aligned Eigen vector.
     */
    template <typename T>
    using AlignedEigenVector = std::vector<T, Eigen::aligned_allocator<T>>;
}

#endif
