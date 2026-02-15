#include "Sphere.h"

Sphere::Sphere(const Eigen::Vector3d centerIn, float radiusIn){
    center = centerIn;
    radius = radiusIn;
}

bool Sphere::tryIntersectRay(Ray &r, double& t) {
    
    Eigen::Vector3d oc = r.getOrigin() - center;

    double a = r.getDirection().dot(r.getDirection());
    double b = 2.0 * oc.dot(r.getDirection());
    double c = oc.dot(oc) - radius * radius;

    double discriminant = b * b - 4 * a * c;
    if (discriminant < 0) return false;
    double sqrtD = sqrt(discriminant);

    double t0 = (-b - sqrtD) / (2 * a);
    double t1 = (-b + sqrtD) / (2 * a);

    t = (t0 > 0) ? t0 : t1;

    return t > 0;

}