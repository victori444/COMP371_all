#pragma once
#include "Geometry.h"
#include <Eigen/Dense>
#include "Ray.h"

class Sphere : public Geometry {
    public:
        Sphere(const Eigen::Vector3d centerIn, float radiusIn);
        Eigen::Vector3d getCenter() const { return center; }
        float getRadius() const { return radius; }
        Eigen::Vector3d getNormal(const Eigen::Vector3d& p) override;

        bool tryIntersectRay(Ray& r, double& t) override;

    private:
        Eigen::Vector3d center;
        float radius;
};