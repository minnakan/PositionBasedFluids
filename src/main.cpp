#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <openglDebug.h>
#include <iostream>

#include <glm/gtc/type_ptr.hpp>

#include <Camera.h>
#include <Shader.h>

#include "PBFSystem.h"

void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void initParticleBuffers();
void drawParticles(const PBFSystem& pbf, Shader& shader);

//Global Vars
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

Camera camera(
    glm::vec3(0.0f, 5.0f, 15.0f),  // position
    glm::vec3(0.0f, 1.0f, 0.0f),   // world up
    -90.0f,                        // yaw (turn left/right)
    -20.0f                         // pitch (angle down slightly)
);
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

unsigned int particleVAO = 0;
unsigned int particleVBO = 0;

PBFSystem pbf;
Shader* sphereShader;

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

    // Reset simulation with R key
    if (key == GLFW_KEY_R && action == GLFW_PRESS)
        pbf.initScene();
}

int main(void)
{
    if (!glfwInit())
        return -1;

    // Enable OpenGL debugging output
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);

    // Set OpenGL version
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Position Based Fluids", NULL, NULL);
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

    // Create custom shaders for sphere rendering
    sphereShader = new Shader(RESOURCES_PATH"vertex.vert", RESOURCES_PATH"fragment.frag");

    // Initialize particle buffers and the PBF system
    initParticleBuffers();
    pbf.initScene();

    // Main rendering loop
    while (!glfwWindowShouldClose(window))
    {
        // Calculate frame time
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Handle input
        processInput(window);

        // Update simulation
        pbf.step();

        // Clear the frame
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Prepare view and projection matrices
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 1000.0f);
        glm::mat4 model = glm::mat4(1.0f);

        // Configure shader and draw particles
        sphereShader->use();
        sphereShader->setMat4("model", model);
        sphereShader->setMat4("view", view);
        sphereShader->setMat4("projection", projection);
        sphereShader->setVec3("viewPos", camera.Position.x, camera.Position.y, camera.Position.z);
        sphereShader->setVec3("lightPos", 10.0f, 10.0f, 10.0f);
        sphereShader->setFloat("particleRadius", pbf.particleRadius);

        drawParticles(pbf, *sphereShader);

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Clean up
    glDeleteBuffers(1, &particleVBO);
    glDeleteVertexArrays(1, &particleVAO);
    delete sphereShader;

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
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        pbf.gravity = glm::vec3(0.0f, 0.0f, 0.0f); // Zero gravity when space is held
    else if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE)
        pbf.gravity = glm::vec3(0.0f, -9.81f, 0.0f); // Normal gravity when space is released
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

    // We need space for position and color
    struct ParticleVertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    // Allocate memory for all particles
    glBufferData(GL_ARRAY_BUFFER, 100000 * sizeof(ParticleVertex), nullptr, GL_DYNAMIC_DRAW);

    // Set up the vertex attributes
    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)0);

    // Color attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)(sizeof(glm::vec3)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void drawParticles(const PBFSystem& pbf, Shader& shader)
{
    if (pbf.particles.empty()) return;

    // Create a temporary buffer containing positions and colors
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

    // Pass the data to OpenGL
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(ParticleVertex), vertices.data(), GL_DYNAMIC_DRAW);

    // Draw the particles as points (the fragment shader will render them as spheres)
    shader.use();
    glDrawArrays(GL_POINTS, 0, (GLsizei)pbf.particles.size());

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}