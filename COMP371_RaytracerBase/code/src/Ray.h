#ifndef RAY_H
#define RAY_H

#include <Eigen/Dense>

class Ray {
public:
    Ray(Eigen::Vector3d directionIn, Eigen::Vector3d originIn);

    Eigen::Vector3d getDirection();
    Eigen::Vector3d getOrigin();

    // check where a point is along a ray at param t
    Eigen::Vector3d getPointAtT(float t);

private:
    Eigen::Vector3d direction;
    Eigen::Vector3d origin;
};

#endif