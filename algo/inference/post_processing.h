#ifndef POST_PROCESSING_H
#define POST_PROCESSING_H

#include <math.h>
#include <Eigen/Dense>

namespace vuprs
{
    Eigen::Matrix<double, -1, 1> softmax(const Eigen::Matrix<double, -1, 1> &vec);
}

#endif
