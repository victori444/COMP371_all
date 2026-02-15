#include <json.hpp>
#include "Sphere.h"
#include "Ray.h"

class RayTracer {
    public:
        RayTracer(nlohmann::json& j);
        void run();
    private:
        nlohmann::json& sourceFile;
};