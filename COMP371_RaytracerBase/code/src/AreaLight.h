#include "Light.h"

class AreaLight : public Light {
    public:
        Eigen::Vector3d p1, p2, p3, p4;
        Eigen::Vector3d centre;

        AreaLight() {
            // centre is computed from corners
        }

        Eigen::Vector3d getPosition() const override { return centre; }

        Eigen::Vector3d getDirection(const Eigen::Vector3d& hitPoint) const override {
            return (centre - hitPoint).normalized();
        }

        double getDistance(const Eigen::Vector3d& hitPoint) const override {
            return (centre - hitPoint).norm();
        }
};