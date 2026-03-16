#include "Ray.h"

Ray::Ray(Eigen::Vector3d directionIn, Eigen::Vector3d originIn) {
    direction = directionIn.normalized();
    origin = originIn;
}

Eigen::Vector3d Ray::getDirection() {
    return direction;
}
Eigen::Vector3d Ray::getOrigin() {
    return origin;
}

Eigen::Vector3d Ray::getPointAtT(float t) {
    return origin + t * direction;
}