#include "Light.h"

class PointLight : public Light {
    public:
        Eigen::Vector3d position;

        Eigen::Vector3d getPosition() const override { return position; }

        Eigen::Vector3d getDirection(const Eigen::Vector3d& hitPoint) const override {
            return (position - hitPoint).normalized();
        }

        double getDistance(const Eigen::Vector3d& hitPoint) const override {
            return (position - hitPoint).norm();
        }
};