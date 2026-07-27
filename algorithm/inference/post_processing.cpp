#include "algorithm/inference/post_processing.h"

Eigen::Matrix<double, -1, 1> vuprs::softmax(const Eigen::Matrix<double, -1, 1> &vec)
{
    double max_val = vec.maxCoeff();
    Eigen::ArrayXd exp_vals = (vec.array() - max_val).exp();
    return (exp_vals / exp_vals.sum()).matrix();
}
