#define GLEW_STATIC 1
#include "A1solution.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <GL/glew.h>    // Include GLEW - OpenGL Extension Wrangler
#include <GLFW/glfw3.h> // GLFW provides a cross-platform interface for creating a graphical context, initializing OpenGL and binding inputs
#include <glm/glm.hpp>  // GLM is an optimized math library with syntax to similar to OpenGL Shading Language
using namespace std;

A1solution::A1solution() {}

// =============================== //
//           STRUCTURES            //
// =============================== //

// SCENE DATA: All data loaded from the input file
struct SceneData {
    glm::mat4 modelViewMatrix; // object space -> camera space
    glm::mat4 projectionMatrix; // 3D camera space -> 2D viewport
    // Raw mesh geometry
    int width;
    int height;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<unsigned int> indices;
};

/* MESH: GPU-side handle
* VAO (Vertex Array Object) : which buffers are bound + how they're laid out
* VBO (Vertex Buffer Object) : vertex positions on the GPU
* EBO (Element Buffer Object) : triangle indices s.t. vertices can be shared
*/ 
struct Mesh {
    unsigned int VAO;
    unsigned int VBO;
    unsigned int EBO;
    size_t indexCount;
};

/* FLAT MESH: expanded mesh 
* Each triangle gets its own 3 private copies of vertices instead of sharing
*/ 
struct FlatMesh {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> baryCoords;
    // All 3 triangle vertex positions stored per-vertex (repeated for each corner)
    std::vector<glm::vec3> triV0; // position of vertex 0 of the triangle this vertex belongs to
    std::vector<glm::vec3> triV1;
    std::vector<glm::vec3> triV2;
};

struct ShaderPrograms {
    unsigned int phongShader;
    unsigned int flatShader;
    unsigned int circleShader;
    unsigned int voronoiShader;
};

// =============================== //
//        HELPER FUNCTIONS         //
// =============================== //

/**
 * Raw file data (positions only)
         │
         ▼
  calculateNormals()
  → computes smooth per-vertex normals from face geometry
         │
         ▼
  createFlatMesh()
  → expands shared vertices into per-triangle private copies
  → attaches barycentric coords + all 3 triangle corners to each vertex
         │
         ▼
  createMeshWithBary()
  → uploads all 6 data streams to the GPU into a VAO
         │
         ▼
  Circle/Voronoi shaders can now compute incircles and Voronoi regions
  because every fragment knows its triangle's full geometry
 */

/**
 * CALCULATE NORMALS
 * Calculates what way each surface is pointing given input file's vertex positions
 */
std::vector<glm::vec3> calculateNormals(const std::vector<glm::vec3>& vertices, const std::vector<unsigned int>& indices) {
    
    // 1 normal slot / vertex initialized to (0,0,0)
    std::vector<glm::vec3> normals(vertices.size(), glm::vec3(0.0f));

    // Loop over every triangle (every 3 indices)
    for (size_t i = 0; i < indices.size(); i += 3) {
        // Fetch 3D positions of triangle's corners
        int i0 = indices[i];
        int i1 = indices[i+1];
        int i2 = indices[i+2];
        glm::vec3 v0 = vertices[i0];
        glm::vec3 v1 = vertices[i1];
        glm::vec3 v2 = vertices[i2];

        // Triangles' edge vectors by subtracting 2 vertices
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;

        // Face normal: cross product of 2 edges (perpendicular) & shrunk to length=1
        glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

        // Accumulated normals since vertices are shared
        normals[i0] += faceNormal;
        normals[i1] += faceNormal;
        normals[i2] += faceNormal;
    }

    // normalize accumulated normals back to unit length
    // Total: average of all surrounding face normals => smooth shading
    for (size_t i = 0; i < normals.size(); ++i) {
        normals[i] = glm::normalize(normals[i]);
    }

    return normals;
}

/**
 * CREATE FLAT MESH
 * Gives every triangle its own private copy of data for Circle & Voronoi shaders
 * that need to know all 3 vertex positions of a triangle
 */
FlatMesh createFlatMesh(const SceneData& scene) {

    // Barycentric coordinates of 3 vertices
    // Position inside a triangle as a weighted blend of its 3 corners
    FlatMesh flat;
    static const glm::vec3 bary[3] = {
        {1,0,0}, {0,1,0}, {0,0,1}
    };

    for (size_t i = 0; i < scene.indices.size(); i += 3) {

        unsigned int i0 = scene.indices[i + 0];
        unsigned int i1 = scene.indices[i + 1];
        unsigned int i2 = scene.indices[i + 2];

        // All 3 corner positions of triangle
        glm::vec3 p0 = scene.vertices[i0];
        glm::vec3 p1 = scene.vertices[i1];
        glm::vec3 p2 = scene.vertices[i2];

        // For each triangle, all 3 of its vertices get the same p0,p1,p2 values
        for (int j = 0; j < 3; j++) {
            unsigned int idx = scene.indices[i + j];
            flat.vertices.push_back(scene.vertices[idx]); // current vertex's position
            flat.normals.push_back(scene.normals[idx]); // current vertex/s normal
            flat.baryCoords.push_back(bary[j]); // (1,0,0) or (0,1,0) or (0,0,1)
            // Every vertex in this triangle knows all 3 triangle corners
            flat.triV0.push_back(p0);
            flat.triV1.push_back(p1);
            flat.triV2.push_back(p2);
        }
    }
    return flat;
}

/**
 * CREATE MESH WITH BARY
 * Uploads everything to the GPU
 */
Mesh createMeshWithBary(const FlatMesh& flat) {
    Mesh mesh;
    mesh.indexCount = flat.vertices.size();

    glGenVertexArrays(1, &mesh.VAO);
    glBindVertexArray(mesh.VAO);

    // Upload helper to call on each data stream
    auto uploadVec3Buffer = [](const std::vector<glm::vec3>& data, int attribLoc) {
        unsigned int buf;
        glGenBuffers(1, &buf); // create buffer on gpu & bind
        glBindBuffer(GL_ARRAY_BUFFER, buf);
        // copy cpu data to gpu
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(glm::vec3), data.data(), GL_STATIC_DRAW);
        // slot N = this buffer w/ 3 floats each
        glVertexAttribPointer(attribLoc, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        // activate slot
        glEnableVertexAttribArray(attribLoc);
        return buf;
    };

    uploadVec3Buffer(flat.vertices,0); // aPos
    uploadVec3Buffer(flat.normals,1); // aNormal
    uploadVec3Buffer(flat.baryCoords,2); // aBaryCoord
    uploadVec3Buffer(flat.triV0,3); // aTriV0
    uploadVec3Buffer(flat.triV1,4); // aTriV1
    uploadVec3Buffer(flat.triV2,5); // aTriV2

    glBindVertexArray(0);
    mesh.EBO = 0; // flat mesh has no shared vertices -> no index buffer (vertices read sequentially)
    return mesh;
}

// =============================== //
//       MAIN FUNCTION BLOCKS      //
// =============================== //

/**
 * LOAD SCENE FROM FILE
 * Reads 3D scene from txt file into SceneData object
 * Data: camera, window, mesh geometry
 */
SceneData loadSceneFromFile(const std::string& filename) {

    SceneData scene;

    // Open input file
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Can not open file\n";
        exit(EXIT_FAILURE);
    }

    // Modelview matrix
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            file >> scene.modelViewMatrix[i][j];
        }
    }
    // Projection matrix
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            file >> scene.projectionMatrix[i][j];
        }
    }

    // window height & width
    file >> scene.width >> scene.height;

    // GEOMETRY
    // N: amount of vertices in mesh (read in 3's)
    int N;
    file >> N;
    scene.vertices.resize(N);
    for (int i = 0; i < N; i++) {
        file >> scene.vertices[i].x >> scene.vertices[i].y >> scene.vertices[i].z;
    }

    // M: number of triangles (M*3 index values)
    int M;
    file >> M;
    scene.indices.resize(M * 3);
    for (int i = 0; i < M * 3; i++) {
        file >> scene.indices[i];
    }

    // Compute all normals in scene
    scene.normals = calculateNormals(scene.vertices, scene.indices);

    file.close();
    return scene;
}

/**
 * INIT OPEN GL
 * Starts graphics system & opens window
 */
GLFWwindow* initOpenGL(int width, int height) {

    // Initialize GLFW and OpenGL version
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // Create Window and rendering context using GLFW
    GLFWwindow* window = glfwCreateWindow(width, height, "COMP371_Assignment_1", NULL, NULL);
    if (window == NULL)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return nullptr;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = true; // Needed for core profile
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to create GLEW" << std::endl;
        glfwTerminate();
        return nullptr;
    }

    // framebuffer vs window size s.t. mapping is correct
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight); // real pixel count
    glViewport(0, 0, fbWidth, fbHeight);
    
    // depth test
    glEnable(GL_DEPTH_TEST);

    return window;
}

/**
 * CREATE MESH
 * Upload simple mesh to GPU (positions, normals, index buffer)
 */
Mesh createMesh(
    const std::vector<glm::vec3>& vertices, 
    const std::vector<glm::vec3>& normals,
    const std::vector<unsigned int>& indices
) {
    
    Mesh mesh;
    mesh.indexCount = indices.size();

    glGenVertexArrays(1, &mesh.VAO);
    glBindVertexArray(mesh.VAO);

    // Vertex position buffer
    glGenBuffers(1, &mesh.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    // Normals buffer
    unsigned int normalBuffer;
    glGenBuffers(1, &normalBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, normalBuffer);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(1);

    // Index buffer
    glGenBuffers(1, &mesh.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);

    return mesh;
}

/**
 * COMPILE & LINK SHADERS
 * Compile shader code & build a GPU program, return shader program id
 */
int compileAndLinkShaders(const char* vertexShaderSource, const char* fragmentShaderSource)
{

    // compile vertex shader
    int vertexShader = glCreateShader(GL_VERTEX_SHADER); // empty shader object on GPU
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL); // give GLSL source code string
    glCompileShader(vertexShader); // compile

    // check for shader compile errors while GPU driver compiles shaders
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // compile fragment shader
    int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // check for shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // link shaders (vertex & fragment shaders into 1 program)
    int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// =============================== //
// ========== SHADERS ============ //
// =============================== //

const char* getVertexShaderSource()
{
    return
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "uniform mat4 modelViewMatrix;\n"
    "uniform mat4 projectionMatrix;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = projectionMatrix * modelViewMatrix * vec4(aPos, 1.0);\n"
    "}\n";
}

const char* getFragmentShaderSource()
{
    return
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(1.0, 0.5, 0.5, 1.0);\n"
    "}\n";
}

// =============================== //
//             PHONG               //
// =============================== //

const char* getPhongVertexShader() {
    return
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aNormal;\n"

    "uniform mat4 modelViewMatrix;\n"
    "uniform mat4 projectionMatrix;\n"

    "out vec3 FragPos;\n"
    "out vec3 Normal;\n"

    "void main()\n"
    "{\n"
    "   FragPos = vec3(modelViewMatrix * vec4(aPos, 1.0));\n"
    "   Normal = mat3(transpose(inverse(modelViewMatrix))) * aNormal;\n"
    "   gl_Position = projectionMatrix * vec4(FragPos, 1.0);\n"
    "}\n";
}

const char* getPhongFragmentShader()
{
    return
    "#version 330 core\n"
    "in vec3 FragPos;\n"
    "in vec3 Normal;\n"

    "out vec4 FragColor;\n"

    "uniform vec3 lightPos;\n"
    "uniform vec3 viewPos;\n"
    "uniform vec3 lightColor;\n"

    "void main()\n"
    "{\n"
    "   vec3 diffuseColor  = vec3(1.0, 0.5, 0.5);\n"
    "   vec3 ambientColor  = vec3(0.1, 0.05, 0.05);\n"
    "   vec3 specularColor = vec3(0.3, 0.3, 0.3);\n"

    "   vec3 norm = normalize(Normal);\n"
    "   vec3 lightDir = normalize(lightPos - FragPos);\n"
    "   vec3 viewDir  = normalize(viewPos - FragPos);\n"
    "   vec3 reflectDir = reflect(-lightDir, norm);\n"

    "   float diff = max(dot(norm, lightDir), 0.0);\n"
    "   float spec = pow(max(dot(viewDir, reflectDir), 0.0), 5.0);\n"

    "   vec3 ambient  = ambientColor * lightColor;\n"
    "   vec3 diffuse  = diff * diffuseColor * lightColor;\n"
    "   vec3 specular = spec * specularColor * lightColor;\n"

    "   FragColor = vec4(ambient + diffuse + specular, 1.0);\n"
    "}\n";
}

// =============================== //
//             FLAT                //
// =============================== //

const char* getFlatVertexShader() {
    return
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"

    "uniform mat4 modelViewMatrix;\n"
    "uniform mat4 projectionMatrix;\n"

    "out vec3 FragPos;\n"

    "void main()\n"
    "{\n"
    "   FragPos = vec3(modelViewMatrix * vec4(aPos, 1.0));\n"
    "   gl_Position = projectionMatrix * vec4(FragPos, 1.0);\n"
    "}\n";
}

const char* getFlatFragmentShader()
{
    return
    "#version 330 core\n"
    "in vec3 FragPos;\n"
    "out vec4 FragColor;\n"

    "uniform vec3 lightPos;\n"
    "uniform vec3 viewPos;\n"
    "uniform vec3 lightColor;\n"

    "void main()\n"
    "{\n"
    "   vec3 dx = dFdx(FragPos);\n"
    "   vec3 dy = dFdy(FragPos);\n"
    "   vec3 normal = normalize(cross(dx, dy));\n"

    "   vec3 diffuseColor  = vec3(1.0, 0.5, 0.5);\n"
    "   vec3 ambientColor  = vec3(0.1, 0.05, 0.05);\n"
    "   vec3 specularColor = vec3(0.3, 0.3, 0.3);\n"

    "   vec3 lightDir = normalize(lightPos - FragPos);\n"
    "   vec3 viewDir  = normalize(viewPos - FragPos);\n"
    "   vec3 reflectDir = reflect(-lightDir, normal);\n"

    "   float diff = max(dot(normal, lightDir), 0.0);\n"
    "   float spec = pow(max(dot(viewDir, reflectDir), 0.0), 5.0);\n"

    "   vec3 ambient  = ambientColor * lightColor;\n"
    "   vec3 diffuse  = diff * diffuseColor * lightColor;\n"
    "   vec3 specular = spec * specularColor * lightColor;\n"

    "   FragColor = vec4(ambient + diffuse + specular, 1.0);\n"
    "}\n";
}

// =============================== //
//             CIRCLE              //
// =============================== //

const char* getCircleVertexShader() {
    return
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aNormal;\n"
    "layout (location = 2) in vec3 aBaryCoord;\n"
    "layout (location = 3) in vec3 aTriV0;\n"
    "layout (location = 4) in vec3 aTriV1;\n"
    "layout (location = 5) in vec3 aTriV2;\n"

    "uniform mat4 modelViewMatrix;\n"
    "uniform mat4 projectionMatrix;\n"

    "out vec3 FragPos;\n"
    "out vec3 Normal;\n"
    "flat out vec3 TriV0;\n"
    "flat out vec3 TriV1;\n"
    "flat out vec3 TriV2;\n"

    "void main()\n"
    "{\n"
    "   FragPos = vec3(modelViewMatrix * vec4(aPos, 1.0));\n"
    "   Normal  = mat3(transpose(inverse(modelViewMatrix))) * aNormal;\n"
    // Transform triangle vertices to view space
    "   TriV0 = vec3(modelViewMatrix * vec4(aTriV0, 1.0));\n"
    "   TriV1 = vec3(modelViewMatrix * vec4(aTriV1, 1.0));\n"
    "   TriV2 = vec3(modelViewMatrix * vec4(aTriV2, 1.0));\n"
    "   gl_Position = projectionMatrix * vec4(FragPos, 1.0);\n"
    "}\n";
}

const char* getCircleFragmentShader() {
    return
    "#version 330 core\n"
    "in vec3 FragPos;\n"
    "in vec3 Normal;\n"
    "flat in vec3 TriV0;\n"
    "flat in vec3 TriV1;\n"
    "flat in vec3 TriV2;\n"

    "out vec4 FragColor;\n"

    "uniform vec3 lightPos;\n"
    "uniform vec3 viewPos;\n"
    "uniform vec3 lightColor;\n"

    "void main()\n"
    "{\n"
    // Compute edge lengths (side opposite to each vertex)
    "   float a = length(TriV1 - TriV2);\n" // opposite V0
    "   float b = length(TriV0 - TriV2);\n" // opposite V1
    "   float c = length(TriV0 - TriV1);\n" // opposite V2

    // Incenter: weighted average of vertices by opposite edge lengths
    "   float perimeter = a + b + c;\n"
    "   vec3 incenter = (a * TriV0 + b * TriV1 + c * TriV2) / perimeter;\n"

    // Inradius: r = Area / semi-perimeter
    "   float s = perimeter * 0.5;\n"
    "   float area = length(cross(TriV1 - TriV0, TriV2 - TriV0)) * 0.5;\n"
    "   float inradius = area / s;\n"

    // Distance from fragment to incenter (in 3D view space)
    "   float dist = length(FragPos - incenter);\n"

    "   vec3 diffuseColor;\n"
    "   vec3 ambientColor;\n"
    "   vec3 specularColor;\n"

    "   if (dist <= inradius) {\n"
    "       diffuseColor  = vec3(1.0, 0.5, 0.5);\n"
    "       ambientColor  = vec3(0.1, 0.05, 0.05);\n"
    "       specularColor = vec3(0.3, 0.3, 0.3);\n"
    "   } else {\n"
    "       diffuseColor  = vec3(0.5, 0.5, 1.0);\n"
    "       ambientColor  = vec3(0.05, 0.05, 0.1);\n"
    "       specularColor = vec3(0.0);\n"
    "   }\n"

    "   vec3 norm = normalize(Normal);\n"
    "   vec3 lightDir   = normalize(lightPos - FragPos);\n"
    "   vec3 viewDir    = normalize(viewPos  - FragPos);\n"
    "   vec3 reflectDir = reflect(-lightDir, norm);\n"

    "   float diff = max(dot(norm, lightDir), 0.0);\n"
    "   float spec = pow(max(dot(viewDir, reflectDir), 0.0), 5.0);\n"

    "   vec3 ambient  = ambientColor  * lightColor;\n"
    "   vec3 diffuse  = diff * diffuseColor  * lightColor;\n"
    "   vec3 specular = spec * specularColor * lightColor;\n"

    "   FragColor = vec4(ambient + diffuse + specular, 1.0);\n"
    "}\n";
}


// =============================== //
//             VORONOI             //
// =============================== //

const char* getVoronoiVertexShader() {
    return
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aNormal;\n"
    "layout (location = 2) in vec3 aBaryCoord;\n"
    "layout (location = 3) in vec3 aTriV0;\n"
    "layout (location = 4) in vec3 aTriV1;\n"
    "layout (location = 5) in vec3 aTriV2;\n"

    "uniform mat4 modelViewMatrix;\n"
    "uniform mat4 projectionMatrix;\n"

    "out vec3 FragPos;\n"
    "out vec3 Normal;\n"
    "flat out vec3 TriV0;\n"
    "flat out vec3 TriV1;\n"
    "flat out vec3 TriV2;\n"

    "void main()\n"
    "{\n"
    "   FragPos = vec3(modelViewMatrix * vec4(aPos, 1.0));\n"
    "   Normal = mat3(transpose(inverse(modelViewMatrix))) * aNormal;\n"
    "   TriV0 = vec3(modelViewMatrix * vec4(aTriV0, 1.0));\n"
    "   TriV1 = vec3(modelViewMatrix * vec4(aTriV1, 1.0));\n"
    "   TriV2 = vec3(modelViewMatrix * vec4(aTriV2, 1.0));\n"
    "   gl_Position = projectionMatrix * vec4(FragPos, 1.0);\n"
    "}\n";
}

const char* getVoronoiFragmentShader() {
    return
    "#version 330 core\n"
    "in vec3 FragPos;\n"
    "in vec3 Normal;\n"
    "flat in vec3 TriV0;\n"
    "flat in vec3 TriV1;\n"
    "flat in vec3 TriV2;\n"

    "out vec4 FragColor;\n"

    "uniform vec3 lightPos;\n"
    "uniform vec3 viewPos;\n"
    "uniform vec3 lightColor;\n"

    "void main()\n"
    "{\n"
    "   float d0 = distance(FragPos, TriV0);\n"
    "   float d1 = distance(FragPos, TriV1);\n"
    "   float d2 = distance(FragPos, TriV2);\n"

    "   vec3 diffuseColor;\n"
    "   if (d0 <= d1 && d0 <= d2) {\n"
    "       diffuseColor = vec3(1.0, 0.5, 0.5);\n"
    "   } else if (d1 <= d2) {\n"
    "       diffuseColor = vec3(0.5, 1.0, 0.5);\n"
    "   } else {\n"
    "       diffuseColor = vec3(0.5, 0.5, 1.0);\n"
    "   }\n"

    "   vec3 ambientColor  = vec3(0.1, 0.05, 0.05);\n"
    "   vec3 specularColor = vec3(0.3, 0.3, 0.3);\n"

    "   vec3 norm = normalize(Normal);\n"
    "   vec3 lightDir = normalize(lightPos - FragPos);\n"
    "   vec3 viewDir = normalize(viewPos - FragPos);\n"
    "   vec3 reflectDir = reflect(-lightDir, norm);\n"

    "   float diff = max(dot(norm, lightDir), 0.0);\n"
    "   float spec = pow(max(dot(viewDir, reflectDir), 0.0), 5.0);\n"

    "   vec3 ambient  = ambientColor * lightColor;\n"
    "   vec3 diffuse  = diff * diffuseColor * lightColor;\n"
    "   vec3 specular = spec * specularColor * lightColor;\n"

    "   FragColor = vec4(ambient + diffuse + specular, 1.0);\n"
    "}\n";
}

// =============================== //
// ========== MAIN RUN =========== //
// =============================== //

void A1solution::run(const std::string& filename) {

    // =============================== //
    //              SETUP              //
    // =============================== //
    // Load scene & open window
    SceneData scene = loadSceneFromFile(filename);
    GLFWwindow* window = initOpenGL(scene.width, scene.height);
    if (!window) return;

    // Compile and link all shaders
    int phongShader  = compileAndLinkShaders(getPhongVertexShader(),  getPhongFragmentShader());
    int flatShader   = compileAndLinkShaders(getFlatVertexShader(),   getFlatFragmentShader());
    int circleShader = compileAndLinkShaders(getCircleVertexShader(), getCircleFragmentShader());
    int voronoiShader = compileAndLinkShaders(getVoronoiVertexShader(), getVoronoiFragmentShader());

    // Store shaders in list to cycle through
    std::vector<int> shaders = {phongShader, flatShader, circleShader, voronoiShader};
    int shaderIndex = 0;
    int activeShader = shaders[shaderIndex];

    // Create & upload meshe types to GPU
    // mesh: index mesh used for Phong & flat (positions & normals)
    // circleMesh: flat/expanded mesh used for Circle & voronoi (all 3 triangle corners/fragment)
    Mesh mesh = createMesh(scene.vertices, scene.normals, scene.indices);
    FlatMesh flat = createFlatMesh(scene);
    Mesh circleMesh = createMeshWithBary(flat);

    // User input flags
    bool sKeyWasPressed = false;
    bool wKeyWasPressed = false;
    bool wireframe = false;

    // =============================== //
    //            MAIN LOOP            //
    // =============================== //
    // Looping every frame until window closed //
    while (!glfwWindowShouldClose(window))
    {

        // =============================== //
        //            USER INPUT           //
        // =============================== //
        glfwPollEvents();

        // Toggle shader with 's'
        bool sKeyDown = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        if (sKeyDown && !sKeyWasPressed) {
            shaderIndex = (shaderIndex + 1) % shaders.size();
            activeShader = shaders[shaderIndex];
        }
        sKeyWasPressed = sKeyDown;

        // Toggle wireframe with 'w'
        bool wKeyDown = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        if (wKeyDown && !wKeyWasPressed) {
            wireframe = !wireframe;
            glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        }
        wKeyWasPressed = wKeyDown;

        // Escape to close
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Clear previous frame by clearing buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Activate shader
        glUseProgram(activeShader);

        // =============================== //
        //           SET UNIFORMS          //
        // =============================== //

        // Lookup variable by name in compiled shader and returned slot number has value written into it
        glUniformMatrix4fv(glGetUniformLocation(activeShader, "modelViewMatrix"),  1, GL_FALSE, &scene.modelViewMatrix[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(activeShader, "projectionMatrix"), 1, GL_FALSE, &scene.projectionMatrix[0][0]);

        glUniform3f(glGetUniformLocation(activeShader, "lightPos"), 0.0f, 0.0f, 0.0f);
        glUniform3f(glGetUniformLocation(activeShader, "viewPos"),  0.0f, 0.0f, 0.0f);
        glUniform3f(glGetUniformLocation(activeShader, "lightColor"),  1.0f, 1.0f, 1.0f);

        // Draw mesh depending on active shader
        if (activeShader == circleShader || activeShader == voronoiShader) {
            glBindVertexArray(circleMesh.VAO);
            glDrawArrays(GL_TRIANGLES, 0, circleMesh.indexCount);
        } else {
            glBindVertexArray(mesh.VAO);
            glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return;
}
