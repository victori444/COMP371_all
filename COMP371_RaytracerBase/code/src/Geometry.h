#pragma once
#include "Ray.h"

class Geometry {
    public:
        double ka, kd, ks, pc;
        Eigen::Vector3d ac, dc, sc;

        virtual bool tryIntersectRay(Ray &r, double& t) = 0;
        virtual Eigen::Vector3d getNormal(const Eigen::Vector3d& p) = 0;
        virtual ~Geometry() {};

};