#define _USE_MATH_DEFINES
#include "RayTracer.h"
#include "Hit.h"
#include "Light.h"
#include "../external/simpleppm.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>

// HELPER FUNCTION //
// Convert JSON array [x,y,z] to Eigen::Vector3d vector
Eigen::Vector3d jsonToVector(const nlohmann::json& j) {
    return Eigen::Vector3d(
        j[0].get<double>(), 
        j[1].get<double>(), 
        j[2].get<double>()
    );
}

// Ray tracer constructor
RayTracer::RayTracer(nlohmann::json& j) : sourceFile(j) {}

// Main run function
void RayTracer::run() {

    // GEOMETRY //

    // get geometry from json
    auto geometry = sourceFile["geometry"];
    std::vector<std::shared_ptr<Geometry>> objects;

    // iterate through each geometry in the scene
    for (auto& g : geometry) {

        std::string type = g["type"];

        // Add geometries objects to respective array 
        // depending on type defined in json
        if (type == "sphere") {
            Eigen::Vector3d centre = jsonToVector(g["centre"]);
            float radius = g["radius"];

            auto sphere = std::make_shared<Sphere>(centre, radius);

            // sphere material
            sphere->ka = g["ka"];
            sphere->kd = g["kd"];
            sphere->ks = g["ks"];
            sphere->pc = g["pc"];

            sphere->ac = jsonToVector(g["ac"]);
            sphere->dc = jsonToVector(g["dc"]);
            sphere->sc = jsonToVector(g["sc"]);

            objects.push_back(sphere);
        }

        if (type == "rectangle") {
            Eigen::Vector3d p1 = jsonToVector(g["p1"]);
            Eigen::Vector3d p2 = jsonToVector(g["p2"]);
            Eigen::Vector3d p3 = jsonToVector(g["p3"]);
            Eigen::Vector3d p4 = jsonToVector(g["p4"]);

            auto triangle1 = std::make_shared<Triangle>(p1, p2, p3);
            auto triangle2 = std::make_shared<Triangle>(p1, p3, p4);

            for (auto triangle : {triangle1, triangle2}) {
                triangle->ka = g["ka"];
                triangle->kd = g["kd"];
                triangle->ks = g["ks"];
                triangle->pc = g["pc"];
                triangle->ac = jsonToVector(g["ac"]);
                triangle->dc = jsonToVector(g["dc"]);
                triangle->sc = jsonToVector(g["sc"]);
            }

            objects.push_back(triangle1);
            objects.push_back(triangle2);
        }
    }

    // LIGHT //

    std::vector<Light> lights;

    for (auto& sceneLight : sourceFile["light"]) {
        if (sceneLight["type"] == "point") {

            Light light;
            light.position = jsonToVector(sceneLight["centre"]);
            light.id = jsonToVector(sceneLight["id"]);
            light.is = jsonToVector(sceneLight["is"]);
            lights.push_back(light);

        }
    }

    // OUTPUT PARAMETERS //

    auto output = sourceFile["output"][0];

    // image resolution in pixels
    auto size = output["size"];
    int width = size[0];
    int height = size[1];

    // camera
    Eigen::Vector3d cameraCentre = jsonToVector(output["centre"]);
    Eigen::Vector3d lookAt = jsonToVector(output["lookat"]);
    Eigen::Vector3d up = jsonToVector(output["up"]);
    float fov = output["fov"];

    // Determine hit colour (default black, else white if black background)
    Eigen::Vector3d backgroundColour = jsonToVector(output["bkc"]);
    Eigen::Vector3d hitColour(0, 0, 0);
    if (backgroundColour.norm() < 1e-6) {
        hitColour = Eigen::Vector3d(1, 1, 1);
    }

    // camera coordinate system
    Eigen::Vector3d forward = lookAt.normalized(); // direction camera looks at
    Eigen::Vector3d right = forward.cross(up).normalized(); // perpendicular to forward & up
    Eigen::Vector3d trueUp = right.cross(forward).normalized(); // vertical axis

    // image plane dimensions
    double fovRadians = fov * M_PI / 180.0; // convert FOV from degrees to radians
    double planeHeight = 2.0 * tan(fovRadians / 2.0); // Plane height wrt vertical FOV
    double aspectRatio = (double)width / height;
    double planeWidth = aspectRatio * planeHeight;

    // frame buffer [r,g,b,r,g,b,...]
    std::vector<double> framebuffer(3 * width * height);

    // ray generation loop
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            // convert px coordinates to NDC [0,1]
            double u = (x + 0.5) / width;
            double v = (y + 0.5) / height;

            // NDC to screen space [-1, 1] wrt image plane
            double screenX = (2 * u - 1) * planeWidth / 2;
            double screenY = (1 - 2 * v) * planeHeight / 2; // inverts y for img coordinates

            // determine direction of ray from camera through pixel on img plane, construct ray
            Eigen::Vector3d direction = (forward + (screenX * right) + (screenY * trueUp)).normalized();
            Ray ray(direction, cameraCentre);

            // Check if intersection exists at pixel
            Eigen::Vector3d pixelColour = backgroundColour;
            double EPS = 1e-6;
            double closestT = 1e9;
            bool hit = false;
            Geometry* hitObject = nullptr;

            // object intersection
            for (auto& obj : objects) {
                double t;
                if (obj->tryIntersectRay(ray, t)) {
                    if (t > EPS && t < closestT) {
                        closestT = t;
                        // pixelColour = hitColour; // set colour if intersection
                        hitObject = obj.get();
                        hit = true;
                    }
                }
            } 

            // final pixel colour
            if (hit) {

                Eigen::Vector3d hitPoint = cameraCentre + closestT * direction;
                Eigen::Vector3d N = hitObject->getNormal(hitPoint).normalized();
                Eigen::Vector3d V = (cameraCentre - hitPoint).normalized();
                Eigen::Vector3d colour(0,0,0);

                // AMBIENT LIGHT
                Eigen::Vector3d ambientIntensity = jsonToVector(output["ai"]);
                colour += hitObject->ka * hitObject->ac.cwiseProduct(ambientIntensity);

                // loop through lights
                for (auto& light : lights) {
                    Eigen::Vector3d L = (light.position - hitPoint).normalized();
                    double lightDistance = (light.position - hitPoint).norm();

                    // SHADOW
                    Ray shadowRay(L, hitPoint + EPS * N);
                    bool inShadow = false;
                    
                    for (auto& obj : objects) {
                        double t;
                        if (obj->tryIntersectRay(shadowRay, t)) {
                            if (t > EPS && t < lightDistance) {
                                inShadow = true;
                                break;
                            }
                        }
                    }

                    if (inShadow) continue;

                    // DIFFUSE LIGHT

                    bool twoSidedRendering = output.value("twosiderender", true);
                    double NdotL = twoSidedRendering ? std::abs(N.dot(L)) : std::max(0.0, N.dot(L));
                    Eigen::Vector3d diffuse = hitObject->kd * NdotL * hitObject->dc.cwiseProduct(light.id);

                    // SPECULAR

                    Eigen::Vector3d H = (L + V).normalized();
                    double NdotH = std::max(0.0, N.dot(H));
                    Eigen::Vector3d specular = hitObject->ks * pow(NdotH, hitObject->pc) * hitObject->sc.cwiseProduct(light.is);
                    colour += diffuse + specular;
                }

                pixelColour = colour;
                pixelColour = pixelColour.cwiseMax(0.0).cwiseMin(1.0);
            }

            // store pixels in framebuffer
            int index = 3 * (y * width + x);
            framebuffer[index + 0] = pixelColour[0];
            framebuffer[index + 1] = pixelColour[1];
            framebuffer[index + 2] = pixelColour[2];
        }
    }

    // save final image
    std::string filename = output["filename"];
    save_ppm(filename, framebuffer, width, height);
}