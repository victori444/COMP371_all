#ifndef LIGHT_H
#define LIGHT_H

#include <Eigen/Dense>

struct Light {
    Eigen::Vector3d position;
    Eigen::Vector3d id;
    Eigen::Vector3d is;
};

#endif