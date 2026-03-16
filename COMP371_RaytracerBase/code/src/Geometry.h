#pragma once
#include "Ray.h"

class Geometry {
    public:
        virtual bool tryIntersectRay(Ray &r, double& t) = 0;
        virtual ~Geometry() {};
};