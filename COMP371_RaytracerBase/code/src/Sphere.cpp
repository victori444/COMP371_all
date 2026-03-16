#include "Sphere.h"

// CONSTRUCTOR
Sphere::Sphere(const Eigen::Vector3d centerIn, float radiusIn){
    center = centerIn;
    radius = radiusIn;
}

// TEST IF RAY INTERSECTS WITH SPHERE
bool Sphere::tryIntersectRay(Ray &r, double& t) {
    
    // VECTOR: ray origin -> sphere center
    Eigen::Vector3d oc = r.getOrigin() - center;

    // Quadratic coefficients for intersection with sphere: a*t^2 + b*t + c = 0
    double a = r.getDirection().dot(r.getDirection()); // a = d * d
    double b = 2.0 * oc.dot(r.getDirection()); // b = 2 * D * (0 - C)
    double c = oc.dot(oc) - radius * radius; // c = || 0 - C ||^2 - r^2

    double discriminant = b * b - 4 * a * c;

    // no intersection
    if (discriminant < 0) return false;

    // Compute 2 intersection points along the ray
    double sqrtD = sqrt(discriminant);
    double t0 = (-b - sqrtD) / (2 * a); // closest
    double t1 = (-b + sqrtD) / (2 * a); // furthest

    // Find nearest positive intersection point
    t = (t0 > 0) ? t0 : t1;

    // True if ray hits sphere in front of origin
    return t > 0;

}