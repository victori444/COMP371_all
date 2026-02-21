#pragma once
#include "Geometry.h"
#include <Eigen/Dense>
#include "Ray.h"

class Sphere : public Geometry {
    public:
        Sphere(const Eigen::Vector3d centerIn, float radiusIn);
        Eigen::Vector3d getCenter() const { return center; }
        float getRadius() const { return radius; }

        bool tryIntersectRay(Ray& r, double& t);

    private:
        Eigen::Vector3d center;
        float radius;
};