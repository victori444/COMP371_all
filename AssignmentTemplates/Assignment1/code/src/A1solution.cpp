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

struct ShaderPrograms {
    unsigned int phongShader;
    unsigned int flatShader;
    unsigned int circleShader;
    unsigned int voronoiShader;
};

// =============================== //
//       HELPER DECLARATIONS       //
// =============================== //

// from capsule 2
// const char* getVertexShaderSource();
// const char* getFragmentShaderSource();
// int compileAndLinkShaders(const char* vertexShaderSource, const char* fragmentShaderSource);

// // run() broken down
// SceneData loadSceneFromFile(const std::string& filename);
// GLFWwindow* initOpenGL(int width, int height);
// ShaderPrograms initShaders();
// Mesh createMesh(const std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals, const std::vector<unsigned int>& indices);
// void setMatrices(unsigned int shaderProgram, const glm::mat4& modelViewMatrix, const glm::mat4& projectionMatrix);
// void mainLoop(GLFWwindow* window, ShaderPrograms& shaders, Mesh& mesh, const SceneData& scene);
// void handleInput(GLFWwindow& window, unsigned int& activeShader, ShaderPrograms& shaders);


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
    glViewport(0, 0, width, height);
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
    "uniform vec3 objectColor;\n"

    "void main()\n"
    "{\n"
    "   float ambientStrength = 0.2;\n"
    "   vec3 ambient = ambientStrength * lightColor;\n"

    "   vec3 norm = normalize(Normal);\n"
    "   vec3 lightDir = normalize(lightPos - FragPos);\n"
    "   float diff = max(dot(norm, lightDir), 0.0);\n"
    "   vec3 diffuse = diff * lightColor;\n"

    "   float specularStrength = 0.5;\n"
    "   vec3 viewDir = normalize(viewPos - FragPos);\n"
    "   vec3 reflectDir = reflect(-lightDir, norm);\n"
    "   float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);\n"
    "   vec3 specular = specularStrength * spec * lightColor;\n"

    "   vec3 result = (ambient + diffuse + specular) * objectColor;\n"
    "   FragColor = vec4(result, 1.0);\n"
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
    "uniform vec3 lightColor;\n"
    "uniform vec3 objectColor;\n"

    "void main()\n"
    "{\n"
    "   vec3 dx = dFdx(FragPos);\n"
    "   vec3 dy = dFdy(FragPos);\n"
    "   vec3 normal = normalize(cross(dx, dy));\n"

    "   vec3 lightDir = normalize(lightPos - FragPos);\n"
    "   float diff = max(dot(normal, lightDir), 0.0);\n"

    "   vec3 color = diff * lightColor * objectColor;\n"
    "   FragColor = vec4(color, 1.0);\n"
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
    // int shaderProgram = compileAndLinkShaders(getVertexShaderSource(), getFragmentShaderSource());
    int phongShader = compileAndLinkShaders(getPhongVertexShader(), getPhongFragmentShader());
    int flatShader = compileAndLinkShaders(getFlatVertexShader(), getFlatFragmentShader());
    
    int activeShader = phongShader;

    // create mesh
    Mesh mesh = createMesh(scene.vertices, scene.normals, scene.indices);

    // debug_gl(0);

    // =============================== //
    //           SET UNIFORMS          //
    // =============================== //
    
    glUseProgram(activeShader);
    // GLint modelLoc = glGetUniformLocation(shaderProgram, "modelViewMatrix");
    // GLint projLoc = glGetUniformLocation(shaderProgram, "projectionMatrix");
    // glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &scene.modelViewMatrix[0][0]);
    // glUniformMatrix4fv(projLoc, 1, GL_FALSE, &scene.projectionMatrix[0][0]);

    // Phong uniforms
    // glUniform3f(glGetUniformLocation(phongShader, "lightPos"), 5.0f, 5.0f, 5.0f);
    // glUniform3f(glGetUniformLocation(phongShader, "viewPos"), 0.0f, 0.0f, 5.0f);
    // glUniform3f(glGetUniformLocation(phongShader, "lightColor"), 1.0f, 1.0f, 1.0f);
    // glUniform3f(glGetUniformLocation(phongShader, "objectColor"), 1.0f, 0.5f, 0.3f);

    // =============================== //
    //            MAIN LOOP            //
    // =============================== //
    while(!glfwWindowShouldClose(window))
    {

        // User input
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
            activeShader = phongShader;
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
            activeShader = flatShader;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // Each frame, reset color of each pixel to glClearColor
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(activeShader);

        GLint modelLoc = glGetUniformLocation(activeShader, "modelViewMatrix");
        GLint projLoc = glGetUniformLocation(activeShader, "projectionMatrix");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &scene.modelViewMatrix[0][0]);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, &scene.projectionMatrix[0][0]);

        // Lighting uniforms
        glUniform3f(glGetUniformLocation(activeShader, "lightPos"), 5.0f, 5.0f, 5.0f);
        glUniform3f(glGetUniformLocation(activeShader, "viewPos"), 0.0f, 0.0f, 5.0f);
        glUniform3f(glGetUniformLocation(activeShader, "lightColor"), 1.0f, 1.0f, 1.0f);
        glUniform3f(glGetUniformLocation(activeShader, "objectColor"), 1.0f, 0.5f, 0.3f);

        glBindVertexArray(mesh.VAO); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized

        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
        glfwSwapBuffers(window);

        // Detect inputs
        glfwPollEvents();

    }
    // Shutdown GLFW
    glfwTerminate();
    return;

}
