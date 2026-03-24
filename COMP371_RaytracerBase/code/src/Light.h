#ifndef LIGHT_H
#define LIGHT_H

#include <Eigen/Dense>

class Light {

    public:
        Eigen::Vector3d id;
        Eigen::Vector3d is;
        virtual Eigen::Vector3d getPosition() const = 0;
        virtual Eigen::Vector3d getDirection(const Eigen::Vector3d& hitPoint) const = 0;
        virtual double getDistance(const Eigen::Vector3d& hitPoint) const = 0;
        virtual ~Light() = default;

};

class PointLight : public Light {

    public:
        Eigen::Vector3d position;

        Eigen::Vector3d getPosition() const override { return position; }

        Eigen::Vector3d getDirection(const Eigen::Vector3d& hitPoint) const override {
            return (position - hitPoint).normalized();
        }

        double getDistance(const Eigen::Vector3d& hitPoint) const override {
            return (position - hitPoint).norm();
        }
};

class AreaLight : public Light {
    
    public:
        Eigen::Vector3d p1, p2, p3, p4;
        Eigen::Vector3d centre;

        AreaLight() {}

        Eigen::Vector3d getPosition() const override { return centre; }

        Eigen::Vector3d getDirection(const Eigen::Vector3d& hitPoint) const override {
            return (centre - hitPoint).normalized();
        }

        double getDistance(const Eigen::Vector3d& hitPoint) const override {
            return (centre - hitPoint).norm();
        }
};

#endif