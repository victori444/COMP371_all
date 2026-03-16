#ifndef HIT_H
#define HIT_H

#include <Eigen/Dense>

struct Hit {
    double t;
    Eigen::Vector3d point;
    Eigen::Vector3d normal;
    Geometry* object;
};

#endif