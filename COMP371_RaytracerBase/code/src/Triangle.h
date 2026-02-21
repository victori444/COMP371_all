#pragma once
#include <Eigen/Dense>
#include "Geometry.h"
#include "Ray.h"

class Triangle : public Geometry{
    public:
        Triangle(const Eigen::Vector3d a,
                const Eigen::Vector3d b,
                const Eigen::Vector3d c);

        Eigen::Vector3d getP1() const { return v1; }
        Eigen::Vector3d getP2() const { return v2; }
        Eigen::Vector3d getP3() const { return v3; }
        
        bool tryIntersectRay(Ray& r, double& t);

    private:
        Eigen::Vector3d v1, v2, v3;
};