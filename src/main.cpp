#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <openglDebug.h>
#include <iostream>

#include <glm/gtc/type_ptr.hpp>

#include <Camera.h>
#include <Shader.h>

#include "PBFSystem.h"
#include "WaterRenderer.h"

void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void initParticleBuffers();
void drawParticles(const PBFSystem& pbf, Shader& shader);

// Ground plane functions
void initGroundPlane();
void drawGroundPlane(Shader& shader);
unsigned int planeVAO = 0;
unsigned int planeVBO = 0;

//Global Vars
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

Camera camera(
    glm::vec3(-25.0f, 10.0f, 0.0f),  // position
    glm::vec3(0.0f, 1.0f, 0.0f),   //world up
    0.0f,                        //yaw
    -20.0f                         //pitch 
);
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

unsigned int particleVAO = 0;
unsigned int particleVBO = 0;

float deltaFrameTime = 0.0f;
float lastFrameTime = 0.0f;
int frameCount = 0;
float frameRateUpdateInterval = 1.0f;

PBFSystem pbf;
Shader* sphereShader;
Shader* planeShader;
Shader* directShader;

WaterRenderer* waterRenderer = nullptr;
bool useScreenSpaceWater = true;

#define USE_GPU_ENGINE 0
extern "C"
{
    __declspec(dllexport) unsigned long NvOptimusEnablement = USE_GPU_ENGINE;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = USE_GPU_ENGINE;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);

    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_1: {
            std::cout << "Switching to Dam Break scene\n";
            pbf.initScene(SceneType::DamBreak);
            break;
        }
        case GLFW_KEY_2: {
            std::cout << "Switching to Water Container scene\n";
            pbf.initScene(SceneType::WaterContainer);
            break;
        }
        case GLFW_KEY_3: {
            std::cout << "Switching to Water Container with Dropping Block scene\n";
            pbf.initScene(SceneType::DropBlock);
            break;
        }
        case GLFW_KEY_Q: {
            pbf.toggleWaveMode();
            break;
        }
        case GLFW_KEY_R: {
            std::cout << "Resetting current scene\n";
            pbf.initScene(pbf.currentScene);
            break;
        }
        case GLFW_KEY_SPACE: {
            useScreenSpaceWater = !useScreenSpaceWater;
            std::cout << "Rendering mode: " << (useScreenSpaceWater ? "Screen Space Water" : "Points") << std::endl;
            break;
        }
        }
    }
}

int main(void)
{
    if (!glfwInit())
        return -1;

    // Enable OpenGL debugging output
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);

    // Set OpenGL version
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "PBF Simulation", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    // Set up input callbacks
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSwapInterval(1); // Enable VSync

    // Configure OpenGL
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(glDebugOutput, 0);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //shaders for sphere rendering
    sphereShader = new Shader(RESOURCES_PATH"vertex.vert", RESOURCES_PATH"fragment.frag");
    planeShader = new Shader(RESOURCES_PATH"plane.vert", RESOURCES_PATH"plane.frag");
    directShader = new Shader(RESOURCES_PATH"ssbo_render.vert", RESOURCES_PATH"fragment.frag");

    //particle buffers and the PBF system
    initParticleBuffers();
    initGroundPlane();
    pbf.initScene(SceneType::DamBreak);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    waterRenderer = new WaterRenderer();
    if (!waterRenderer->initialize(SCR_WIDTH, SCR_HEIGHT, pbf.particleRadius)) {
        std::cerr << "Failed to initialize water renderer!" << std::endl;
        delete waterRenderer;
        waterRenderer = nullptr;
    }

    // Main rendering loop
    while (!glfwWindowShouldClose(window))
    {
        //frame time
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        frameCount++;
        deltaFrameTime += deltaTime;

        if (deltaFrameTime >= frameRateUpdateInterval) {
            float fps = frameCount / deltaFrameTime;
            //std::cout << "FPS: " << fps << " (" << (deltaTime * 1000.0f) << " ms/frame)" << std::endl;

            // Reset counters
            frameCount = 0;
            deltaFrameTime = 0.0f;
        }

        //input
        processInput(window);

        //Update simulation
        pbf.step();

        
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),(float)SCR_WIDTH / (float)SCR_HEIGHT,0.1f, 100.0f);
        glm::mat4 model = glm::mat4(1.0f);

        //ground plane
        planeShader->use();
        planeShader->setMat4("model", model);
        planeShader->setMat4("view", view);
        planeShader->setMat4("projection", projection);
        planeShader->setVec3("viewPos", camera.Position.x, camera.Position.y, camera.Position.z);
        planeShader->setVec3("lightPos", 10.0f, 10.0f, 10.0f);
        drawGroundPlane(*planeShader);

		//particle shader - GPU->CPU->GPU
        /*sphereShader->use();
        sphereShader->setMat4("model", model);
        sphereShader->setMat4("view", view);
        sphereShader->setMat4("projection", projection);
        sphereShader->setVec3("viewPos", camera.Position.x, camera.Position.y, camera.Position.z);
        sphereShader->setVec3("lightPos", 10.0f, 10.0f, 10.0f);
        sphereShader->setFloat("particleRadius", pbf.particleRadius);
        drawParticles(pbf, *sphereShader);*/

        if (useScreenSpaceWater && waterRenderer) {
            // Use screen space water rendering
            waterRenderer->renderFluid(pbf, camera, glm::vec3(10.0f, 10.0f, 10.0f));
        }
        else {
            // Use point sprite rendering
            directShader->use();
            directShader->setMat4("model", model);
            directShader->setMat4("view", view);
            directShader->setMat4("projection", projection);
            directShader->setVec3("viewPos", camera.Position.x, camera.Position.y, camera.Position.z);
            directShader->setVec3("lightPos", 10.0f, 10.0f, 10.0f);
            directShader->setFloat("particleRadius", pbf.particleRadius);
            pbf.renderParticlesGPU(camera, SCR_WIDTH, SCR_HEIGHT);
        }

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    if (waterRenderer) {
        waterRenderer->cleanup();
        delete waterRenderer;
    }

    // Clean up
    glDeleteBuffers(1, &particleVBO);
    glDeleteVertexArrays(1, &particleVAO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteVertexArrays(1, &planeVAO);
    delete sphereShader;
    delete planeShader;
    delete directShader;

    glfwTerminate();
    return 0;
}


void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Camera movement
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void initParticleBuffers()
{
    if (particleVAO == 0) glGenVertexArrays(1, &particleVAO);
    if (particleVBO == 0) glGenBuffers(1, &particleVBO);

    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);

    struct ParticleVertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    glBufferData(GL_ARRAY_BUFFER, 10000 * sizeof(ParticleVertex), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)(sizeof(glm::vec3)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // Update viewport
    glViewport(0, 0, width, height);

    // Resize water renderer framebuffers
    if (waterRenderer) {
        waterRenderer->resize(width, height);
    }
}

void drawParticles(const PBFSystem& pbf, Shader& shader)
{
    if (pbf.particles.empty()) return;

    struct ParticleVertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    std::vector<ParticleVertex> vertices;
    vertices.reserve(pbf.particles.size());

    for (const auto& particle : pbf.particles) {
        ParticleVertex vertex;
        vertex.position = particle.position;
        vertex.color = particle.color;
        vertices.push_back(vertex);
    }

    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);

    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(ParticleVertex), vertices.data(), GL_DYNAMIC_DRAW);

    shader.use();
    glDrawArrays(GL_POINTS, 0, (GLsizei)pbf.particles.size());

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void initGroundPlane()
{
    if (planeVAO == 0) glGenVertexArrays(1, &planeVAO);
    if (planeVBO == 0) glGenBuffers(1, &planeVBO);

    // Vertices for a simple quad (ground plane)
    float planeVertices[] = {
        // Positions            // Normals           // Texture coords
        -10.0f, 0.0f, -10.0f,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
         10.0f, 0.0f, -10.0f,   0.0f, 1.0f, 0.0f,   10.0f, 0.0f,
         10.0f, 0.0f,  10.0f,   0.0f, 1.0f, 0.0f,   10.0f, 10.0f,

        -10.0f, 0.0f, -10.0f,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
         10.0f, 0.0f,  10.0f,   0.0f, 1.0f, 0.0f,   10.0f, 10.0f,
        -10.0f, 0.0f,  10.0f,   0.0f, 1.0f, 0.0f,   0.0f, 10.0f
    };

    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    glBindVertexArray(0);
}

void drawGroundPlane(Shader& shader)
{
    shader.use();

    //material properties
    shader.setVec3("planeColor", 0.2f, 0.2f, 0.3f); // Dark blue-gray color

    glBindVertexArray(planeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}