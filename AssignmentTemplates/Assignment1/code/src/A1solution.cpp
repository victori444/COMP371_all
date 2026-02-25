#define GLEW_STATIC 1   // This allows linking with Static Library on Windows, without DLL
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

struct SceneData {
    glm::mat4 modelViewMatrix;
    glm::mat4 projectionMatrix;
    int width;
    int height;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<unsigned int> indices;
};

struct Mesh {
    unsigned int VAO;
    unsigned int VBO;
    unsigned int EBO;
    size_t indexCount;
};

// expand indexed mesh into flat arrays with bary coords
struct FlatMesh {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> baryCoords;
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

std::vector<glm::vec3> calculateNormals(const std::vector<glm::vec3>& vertices, const std::vector<unsigned int>& indices) {
    
    std::vector<glm::vec3> normals(vertices.size(), glm::vec3(0.0f));

    for (size_t i = 0; i < indices.size(); i += 3) {
        int i0 = indices[i];
        int i1 = indices[i+1];
        int i2 = indices[i+2];
        glm::vec3 v0 = vertices[i0];
        glm::vec3 v1 = vertices[i1];
        glm::vec3 v2 = vertices[i2];
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;

        glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));
        normals[i0] += faceNormal;
        normals[i1] += faceNormal;
        normals[i2] += faceNormal;
    }

    // normalize accumulated normals
    for (size_t i = 0; i < normals.size(); ++i) {
        normals[i] = glm::normalize(normals[i]);
    }

    return normals;
}

FlatMesh createFlatMesh(const SceneData& scene) {
    FlatMesh flat;
    static const glm::vec3 bary[3] = {
        {1,0,0}, {0,1,0}, {0,0,1}
    };
    for (size_t i = 0; i < scene.indices.size(); i += 3) {
        for (int j = 0; j < 3; j++) {
            unsigned int idx = scene.indices[i + j];
            flat.vertices.push_back(scene.vertices[idx]);
            flat.normals.push_back(scene.normals[idx]);
            flat.baryCoords.push_back(bary[j]);
        }
    }
    return flat;
}

Mesh createMeshWithBary(const FlatMesh& flat) {
    Mesh mesh;
    mesh.indexCount = flat.vertices.size(); // no EBO needed

    glGenVertexArrays(1, &mesh.VAO);
    glBindVertexArray(mesh.VAO);

    // Positions
    unsigned int vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, flat.vertices.size() * sizeof(glm::vec3), flat.vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    // Normals
    unsigned int nbo;
    glGenBuffers(1, &nbo);
    glBindBuffer(GL_ARRAY_BUFFER, nbo);
    glBufferData(GL_ARRAY_BUFFER, flat.normals.size() * sizeof(glm::vec3), flat.normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(1);

    // Barycentric coords
    unsigned int bbo;
    glGenBuffers(1, &bbo);
    glBindBuffer(GL_ARRAY_BUFFER, bbo);
    glBufferData(GL_ARRAY_BUFFER, flat.baryCoords.size() * sizeof(glm::vec3), flat.baryCoords.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    mesh.EBO = 0;
    return mesh;
}

// =============================== //
//       MAIN FUNCTION BLOCKS      //
// =============================== //

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

    // Geometry: vertices, indices, normals
    int N;
    file >> N;
    scene.vertices.resize(N);
    for (int i = 0; i < N; i++) {
        file >> scene.vertices[i].x >> scene.vertices[i].y >> scene.vertices[i].z;
    }

    int M;
    file >> M;
    scene.indices.resize(M * 3);
    for (int i = 0; i < M * 3; i++) {
        file >> scene.indices[i];
    }

    scene.normals = calculateNormals(scene.vertices, scene.indices);

    file.close();
    return scene;
}

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
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);
    // glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);

    return window;
}

Mesh createMesh(
    const std::vector<glm::vec3>& vertices, 
    const std::vector<glm::vec3>& normals,
    const std::vector<unsigned int>& indices
) {
    
    Mesh mesh;
    mesh.indexCount = indices.size();

    glGenVertexArrays(1, &mesh.VAO);
    glBindVertexArray(mesh.VAO);

    // Vertex buffer
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


int compileAndLinkShaders(const char* vertexShaderSource, const char* fragmentShaderSource)
{
    // compile and link shader program
    // return shader program id
    // ------------------------------------

    // vertex shader
    int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    // check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // fragment shader
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

    // link shaders
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

void createRenderingData(unsigned int& VAO, unsigned int& VBO,unsigned int& CBO, unsigned int PBO[], unsigned int& EBO)
{

    // Define and upload geometry to the GPU here ...
    float vertices[] = {
        -0.5f, -0.5f,
        0.5f,  -0.5f,
        0.0f,  0.75f
    };

    unsigned int indices[] = {  // note that we start from 0!
        0, 1, 2
    };

    float color[] = {
        1.0f,  0.0f, 0.0,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    };

    // 0 - create the vertex array
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // create the vertex buffer
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, 3*2*sizeof(float), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // create the color
    glGenBuffers(1, &CBO);
    glBindBuffer(GL_ARRAY_BUFFER, CBO);
    glBufferData(GL_ARRAY_BUFFER, 3*3*sizeof(float), color, GL_STATIC_DRAW);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);

    glGenBuffers(3, PBO);
    // create the triplet of inputs
    int offset = 2;

    for(int i=0;i<3;++i){

        glBindBuffer(GL_ARRAY_BUFFER, PBO[i]);

        float buffer[6];
        for(int j=0;j<3;++j){
            for(int k=0;k<2;++k){
                buffer[2*j+k] = vertices[2*i+k];
            }
        }

        glBufferData(GL_ARRAY_BUFFER, 3*2*sizeof(float), buffer, GL_STATIC_DRAW);

        glVertexAttribPointer(offset+i, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(offset+i);
    }

    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3*sizeof(unsigned int), indices, GL_STATIC_DRAW);

    // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
    // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    glBindVertexArray(0);
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

// const char* getPhongFragmentShader()
// {
//     return
//     "#version 330 core\n"
//     "in vec3 FragPos;\n"
//     "in vec3 Normal;\n"

//     "out vec4 FragColor;\n"

//     "uniform vec3 lightPos;\n"
//     "uniform vec3 viewPos;\n"
//     "uniform vec3 lightColor;\n"
//     "uniform vec3 objectColor;\n"

//     "void main()\n"
//     "{\n"
//     "   float ambientStrength = 0.2;\n"
//     "   vec3 ambient = ambientStrength * lightColor;\n"

//     "   vec3 norm = normalize(Normal);\n"
//     "   vec3 lightDir = normalize(lightPos - FragPos);\n"
//     "   float diff = max(dot(norm, lightDir), 0.0);\n"
//     "   vec3 diffuse = diff * lightColor;\n"

//     "   float specularStrength = 0.5;\n"
//     "   vec3 viewDir = normalize(viewPos - FragPos);\n"
//     "   vec3 reflectDir = reflect(-lightDir, norm);\n"
//     "   float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);\n"
//     "   vec3 specular = specularStrength * spec * lightColor;\n"

//     "   vec3 result = (ambient + diffuse + specular) * objectColor;\n"
//     "   FragColor = vec4(result, 1.0);\n"
//     "}\n";
// }

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

// const char* getFlatFragmentShader()
// {
//     return
//     "#version 330 core\n"

//     "in vec3 FragPos;\n"
//     "out vec4 FragColor;\n"

//     "uniform vec3 lightPos;\n"
//     "uniform vec3 lightColor;\n"
//     "uniform vec3 objectColor;\n"

//     "void main()\n"
//     "{\n"
//     "   vec3 dx = dFdx(FragPos);\n"
//     "   vec3 dy = dFdy(FragPos);\n"
//     "   vec3 normal = normalize(cross(dx, dy));\n"

//     "   vec3 lightDir = normalize(lightPos - FragPos);\n"
//     "   float diff = max(dot(normal, lightDir), 0.0);\n"

//     "   vec3 color = diff * lightColor * objectColor;\n"
//     "   FragColor = vec4(color, 1.0);\n"
//     "}\n";
// }

// =============================== //
//             CIRCLE              //
// =============================== //

const char* getCircleVertexShader() {
    return
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aNormal;\n"
    "layout (location = 2) in vec3 aBaryCoord;\n"

    "uniform mat4 modelViewMatrix;\n"
    "uniform mat4 projectionMatrix;\n"

    "out vec3 FragPos;\n"
    "out vec3 Normal;\n"
    "out vec3 BaryCoord;\n"

    "void main()\n"
    "{\n"
    "   FragPos = vec3(modelViewMatrix * vec4(aPos, 1.0));\n"
    "   Normal = mat3(transpose(inverse(modelViewMatrix))) * aNormal;\n"
    "   BaryCoord = aBaryCoord;\n"

    "   gl_Position = projectionMatrix * vec4(FragPos, 1.0);\n"
    "}\n";
}

const char* getCircleFragmentShader() {
    return
    "#version 330 core\n"
    "in vec3 FragPos;\n"
    "in vec3 Normal;\n"
    "in vec3 BaryCoord;\n"

    "out vec4 FragColor;\n"

    "uniform vec3 lightPos;\n"
    "uniform vec3 viewPos;\n"
    "uniform vec3 lightColor;\n"

    "void main()\n"
    "{\n"
    "   vec3 diffuseColor  = vec3(1.0, 0.5, 0.5);\n"
    "   vec3 ambientColor  = vec3(0.1, 0.05, 0.05);\n"
    "   vec3 specularColor = vec3(0.3, 0.3, 0.3);\n"

    "   vec3 center = vec3(1.0/3.0);\n"
    "   float radius = 0.40;\n"
    "   float dist = length(BaryCoord - center);\n"

    "   if (dist >= radius) {\n"
    "       diffuseColor  = vec3(0.5, 0.5, 1.0);\n"
    "       ambientColor  = vec3(0.05, 0.05, 0.1);\n"
    "       specularColor = vec3(0.0);\n"
    "   }\n"

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

// const char* getCircleFragmentShader() {
//     return
//     "#version 330 core\n"
//     "in vec3 FragPos;\n"
//     "in vec3 Normal;\n"
//     "in vec3 BaryCoord;\n"

//     "out vec4 FragColor;\n"

//     "uniform vec3 lightPos;\n"
//     "uniform vec3 viewPos;\n"
//     "uniform vec3 lightColor;\n"

//     "void main()\n"
//     "{\n"
//     "   vec3 norm = normalize(Normal);\n"
//     "   vec3 lightDir = normalize(lightPos - FragPos);\n"
//     "   vec3 viewDir = normalize(viewPos - FragPos);\n"
//     "   vec3 reflectDir = reflect(-lightDir, norm);\n"

//     "   float diff = max(dot(norm, lightDir), 0.0);\n"
//     "   float spec = pow(max(dot(viewDir, reflectDir), 0.0), 5.0);\n"

//     "   vec3 diffuseColor = vec3(1.0, 0.5, 0.5);\n"
//     "   vec3 ambientColor = vec3(0.1, 0.05, 0.05);\n"
//     "   vec3 specularColor = vec3(0.3, 0.3, 0.3);\n"

//     "   vec3 center = vec3(1.0/3.0);\n"
//     "   float radius = 0.40;\n"
//     "   float dist = length(BaryCoord - center);\n"

//     "   if (dist < radius) {\n"
//     "       diffuseColor = vec3(0.5, 0.5, 1.0);\n"
//     "       ambientColor = vec3(0.05, 0.05, 0.1);\n"
//     "       specularColor = vec3(0.0);\n"
//     "   }\n"

//     "   vec3 ambient = ambientColor * lightColor;\n"
//     "   vec3 diffuse = diff * diffuseColor * lightColor;\n"
//     "   vec3 specular = spec * specularColor * lightColor;\n"

//     "   vec3 result = ambient + diffuse + specular;\n"
//     "   FragColor = vec4(result, 1.0);\n"
//     "}\n";
// }

// =============================== //
//             VORONOI             //
// =============================== //

const char* getVoronoiVertexShader() {
    return
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aNormal;\n"
    "layout (location = 2) in vec3 aBaryCoord;\n"

    "uniform mat4 modelViewMatrix;\n"
    "uniform mat4 projectionMatrix;\n"

    "out vec3 FragPos;\n"
    "out vec3 Normal;\n"
    "out vec3 BaryCoord;\n"

    "void main()\n"
    "{\n"
    "   FragPos = vec3(modelViewMatrix * vec4(aPos, 1.0));\n"
    "   Normal = mat3(transpose(inverse(modelViewMatrix))) * aNormal;\n"
    "   BaryCoord = aBaryCoord;\n"
    "   gl_Position = projectionMatrix * vec4(FragPos, 1.0);\n"
    "}\n";
}

const char* getVoronoiFragmentShader() {
    return
    "#version 330 core\n"
    "in vec3 FragPos;\n"
    "in vec3 Normal;\n"
    "in vec3 BaryCoord;\n"

    "out vec4 FragColor;\n"

    "uniform vec3 lightPos;\n"
    "uniform vec3 viewPos;\n"
    "uniform vec3 lightColor;\n"

    "void main()\n"
    "{\n"
    // Determine closest vertex by largest barycentric component
    "   vec3 diffuseColor;\n"
    "   if (BaryCoord.x >= BaryCoord.y && BaryCoord.x >= BaryCoord.z) {\n"
    "       diffuseColor = vec3(1.0, 0.5, 0.5);\n"
    "   } else if (BaryCoord.y >= BaryCoord.z) {\n"
    "       diffuseColor = vec3(0.5, 1.0, 0.5);\n"
    "   } else {\n"
    "       diffuseColor = vec3(0.5, 0.5, 1.0);\n"
    "   }\n"

    "   vec3 ambientColor = vec3(0.1, 0.05, 0.05);\n"
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

    SceneData scene = loadSceneFromFile(filename);
    GLFWwindow* window = initOpenGL(scene.width, scene.height);
    if (!window) return;

    // Compile and link shaders
    int phongShader  = compileAndLinkShaders(getPhongVertexShader(),  getPhongFragmentShader());
    int flatShader   = compileAndLinkShaders(getFlatVertexShader(),   getFlatFragmentShader());
    int circleShader = compileAndLinkShaders(getCircleVertexShader(), getCircleFragmentShader());
    int voronoiShader = compileAndLinkShaders(getVoronoiVertexShader(), getVoronoiFragmentShader());

    std::vector<int> shaders = {phongShader, flatShader, circleShader, voronoiShader};
    int shaderIndex = 0;
    int activeShader = shaders[shaderIndex];

    // Create meshes
    Mesh mesh = createMesh(scene.vertices, scene.normals, scene.indices);
    FlatMesh flat = createFlatMesh(scene);
    Mesh circleMesh = createMeshWithBary(flat);

    // Key state tracking (for toggle, not hold)
    bool sKeyWasPressed = false;
    bool wKeyWasPressed = false;
    bool wireframe = false;

    // =============================== //
    //            MAIN LOOP            //
    // =============================== //
    while (!glfwWindowShouldClose(window))
    {
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

        // Clear
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(activeShader);

        // =============================== //
        //           SET UNIFORMS          //
        // =============================== //

        glUniformMatrix4fv(glGetUniformLocation(activeShader, "modelViewMatrix"),  1, GL_FALSE, &scene.modelViewMatrix[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(activeShader, "projectionMatrix"), 1, GL_FALSE, &scene.projectionMatrix[0][0]);

        // glUniform3f(glGetUniformLocation(activeShader, "lightPos"),    5.0f, 5.0f, 5.0f);
        // glUniform3f(glGetUniformLocation(activeShader, "viewPos"),     0.0f, 0.0f, 5.0f);
        glUniform3f(glGetUniformLocation(activeShader, "lightPos"), 0.0f, 0.0f, 0.0f);
        glUniform3f(glGetUniformLocation(activeShader, "viewPos"),  0.0f, 0.0f, 0.0f);
        glUniform3f(glGetUniformLocation(activeShader, "lightColor"),  1.0f, 1.0f, 1.0f);
        // glUniform3f(glGetUniformLocation(activeShader, "objectColor"), 1.0f, 0.5f, 0.3f);

        // Draw
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
