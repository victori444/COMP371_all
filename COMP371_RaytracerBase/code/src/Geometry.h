#include "Ray.h"

class Geometry {
    public:
        virtual bool tryIntersectRay(const Ray &r) = 0;
        virtual ~Geometry();
};