#include "Triangle.h"

// CONSTRUCTOR
Triangle::Triangle(const Eigen::Vector3d a, const Eigen::Vector3d b, const Eigen::Vector3d c) {
    v1 = a;
    v2 = b;
    v3 = c;
}

// TEST IF RAY INTERSECTS WITH SPHERE
bool Triangle::tryIntersectRay(Ray& r, double& t) {
    
    const double EPSILON = 1e-8;

    // triangle edges based on vertices
    Eigen::Vector3d edge1 = v2 - v1;
    Eigen::Vector3d edge2 = v3 - v1;
    Eigen::Vector3d h = r.getDirection().cross(edge2); // cross product: ray direction X edge2

    // is ray parallel to plane (determinant a = 0)
    double a = edge1.dot(h);
    if (fabs(a) < EPSILON) return false;

    double f = 1.0 / a; // inverse deteminant
    Eigen::Vector3d s = r.getOrigin() - v1; // vector from v1 to ray origin

    // u & v: barycentric coordinates
    double u = f * s.dot(h);
    if (u < 0.0 || u > 1.0) return false;
    Eigen::Vector3d q = s.cross(edge1);
    double v = f * r.getDirection().dot(q);

    // intersection outside of the triangle
    if (v < 0.0 || u + v > 1.0) return false;

    // t: distance along ray to intersection
    t = f * edge2.dot(q);
    // if true: intersection in front of ray origin, else, intersection behind (return false)
    if (t > EPSILON) return true;

    return false;
}