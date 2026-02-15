#define _USE_MATH_DEFINES
#include "RayTracer.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>

// each scene = 1 json w/ 1 object

// 1. make a ray class
// 2. intersecting with (unit) spheres 
// 3. intersecting with (unit) triangles -> rectangles
// 4. doing this with json data

// HELPER FUNCTIONS
Eigen::Vector3d jsonToVector(const nlohmann::json& j) {
    return Eigen::Vector3d(j[0].get<double>(), j[1].get<double>(), j[2].get<double>());
}

RayTracer::RayTracer(nlohmann::json& j) : sourceFile(j) {
}

void RayTracer::run() {

    auto geometry = sourceFile["geometry"];
    std::vector<Sphere> spheres;

    for (auto& g : geometry) {
        std::string type = g["type"];
        if (type == "sphere") {
            // auto center = geometry[i]["center"];
            Eigen::Vector3d centre = jsonToVector(g["centre"]);
            float radius = g["radius"];
            spheres.push_back(Sphere(centre, radius));
        }
    }

    auto output = sourceFile["output"][0];

    // resolution
    auto size = output["size"];
    int width = size[0];
    int height = size[1];

    // camera
    Eigen::Vector3d cameraCentre = jsonToVector(output["centre"]);
    Eigen::Vector3d lookAt = jsonToVector(output["lookat"]);
    Eigen::Vector3d up = jsonToVector(output["up"]);
    float fov = output["fov"];
    Eigen::Vector3d backgroundColour = jsonToVector(output["bkc"]);

    // camera vectors
    Eigen::Vector3d forward = (lookAt - cameraCentre).normalized(); // remove cameraCentre if probs w cornell box
    Eigen::Vector3d right = forward.cross(up).normalized();
    Eigen::Vector3d trueUp = right.cross(forward).normalized();

    // image plane dimensions
    double fovRadians = fov * M_PI / 180.0;
    double planeHeight = 2.0 * tan(fovRadians / 2.0);
    double aspectRatio = (double)width / height;
    double planeWidth = aspectRatio * planeHeight;

    // frame buffer
    std::vector<Eigen::Vector3d> framebuffer(width * height, backgroundColour);

    // ray generation
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double u = (x + 0.5) / width;
            double v = (y + 0.5) / height;

            double screenX = (2 * u - 1) * planeWidth / 2;
            double screenY = (1 - 2 * v) * planeHeight / 2;

            Eigen::Vector3d direction = (forward + (screenX * right) + (screenY * trueUp)).normalized();

            Ray ray(direction, cameraCentre);

            // DEBUG
            if (x==width/2 && y==height/2) {
                std::cout << "Center ray direction: " << direction.transpose() << std::endl;
            }

            // Check if intersection exists at pixel
            Eigen::Vector3d pixelColour = backgroundColour;
            double closestT = 1e9;

            for (auto& sphere : spheres) {
                double t;
                if (sphere.tryIntersectRay(ray, t)) {
                    if (t < closestT) {
                        closestT = t;
                        pixelColour = Eigen::Vector3d(0,0,0); // black sphere
                    }
                }
            }

            framebuffer[y * width + x] = pixelColour;
        }
    }

    // write image file (ppm)
    std::string filename = output["filename"];
    std::ofstream out(filename);
    out << "P3\n";
    out << width << " " << height << "\n255\n";
    for (auto& color : framebuffer) {
        int r = std::min(255, (int)(255 * color[0]));
        int g = std::min(255, (int)(255 * color[1]));
        int b = std::min(255, (int)(255 * color[2]));

        out << r << " " << g << " " << b << "\n";  
    }
    out.close();

}