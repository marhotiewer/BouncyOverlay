#define GLFW_EXPOSE_NATIVE_WIN32

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>
#include <iostream>
#include <windows.h>
#include <dwmapi.h>

const char* vertexCircleSource = R"(
    #version 330 core

    uniform mat4                    projection;  // projection matrix uniform

    layout (location = 0) in vec2   basePos;     // original position
    layout (location = 1) in vec2   iPos;        // instanced position
    layout (location = 3) in vec2   iScale;      // instanced scale
    layout (location = 2) in vec3   iColor;      // instanced color
    layout (location = 4) in float  iAngle;      // instanced angle

    out vec2 vPosOut;
    out vec2 vScaleOut;
    out vec3 vColorOut;

    mat2 rotationMatrix(float angle)
    {
        float s = sin(angle);
        float c = cos(angle);
        return mat2(c, -s, s, c);
    }

    void main()
    {
        vec2 scale;

        // if the height of the rectangle is set to 0.0 set the height to equal the width in preperation for rendering as a circle
        if(iScale.y == 0.0) scale = vec2(iScale.x, iScale.x);
        else                scale = iScale;

        // apply rotation and projection maxtrix
        vec2 newPos = rotationMatrix(iAngle) * (basePos * scale);
        gl_Position = projection * vec4(newPos + iPos, 0.0, 1.0);

        vPosOut     = iPos;
        vScaleOut   = iScale;
        vColorOut   = iColor;
    }
)";

const char* fragmentCircleSource = R"(
    #version 330 core

    uniform vec2 resolution;

    in vec3 vColorOut;
    in vec2 vScaleOut;
    in vec2 vPosOut;

    out vec4 FragColor;

    void main()
    {
        // if the height of the square is 0.0 render as circle
        if(vScaleOut.y == 0.0)
        {
            float radius    = vScaleOut.x;
            vec2  centerPos = vPosOut;

            // convert current fragment pixel coordinates to normalized coordinates
            vec2 pixelPos = (2.0 * gl_FragCoord.xy - resolution) / resolution;
            pixelPos.x *= resolution.x / resolution.y;
        
            // dont render the pixel if the position is outside of the circle radius
            if (length(pixelPos - centerPos) > radius) discard;
        }
        FragColor = vec4(vColorOut, 1.0f);
    }
)";

const int   WINDOW_WIDTH = 1920;
const int   WINDOW_HEIGHT = 1080;
const float ASPECT_RATIO = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
bool        WIREFRAME_ENABLED = false;

struct BufferData
{
    const void* data;
    size_t size;
    GLenum usage;
};

struct VertexAttribute
{
    GLuint index;
    GLint size;
    GLenum type;
    GLboolean normalized;
    GLsizei stride;
    const void* offset;
};

struct InstanceData {
    glm::vec2 position;
    glm::vec2 scale;
    glm::vec3 color;
    GLfloat angle;
};

GLuint compileShader(char* vertexSource, char* fragSource)
{
    int  success;
    char infoLog[512];

    // Compile vertex shader
    GLuint vertexShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);

    // Error checking
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "Error, vertex shader failed to compile:\n" << infoLog << std::endl;
    }

    // Compile fragment shader
    GLuint fragmentShader;
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragSource, NULL);
    glCompileShader(fragmentShader);

    // Error checking
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "Error, fragment shader failed to compile:\n" << infoLog << std::endl;
    }

    // Create and link shader program
    GLuint shaderProgram;
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Error checking
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "Error, shaders failed to link:\n" << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

void setupBuffers(GLuint& VAO, GLuint& VBO,
    const BufferData& vertexBufferData,
    const VertexAttribute& vertexAttribute)
{
    // Generate and bind the Vertex Array Object
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Generate and bind the Vertex Buffer Object
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexBufferData.size, vertexBufferData.data, vertexBufferData.usage);

    // Set up vertex attributes for the main vertex data
    glVertexAttribPointer(vertexAttribute.index, vertexAttribute.size, vertexAttribute.type, vertexAttribute.normalized, vertexAttribute.stride, vertexAttribute.offset);
    glEnableVertexAttribArray(vertexAttribute.index);

    // Unbind the Vertex Array Object to avoid accidental modifications
    glBindVertexArray(0);
}

void createEBO(GLuint& EBO, const GLuint& VAO, const BufferData& bufferData)
{
    // Bind the VAO
    glBindVertexArray(VAO);

    // Generate and bind the Index Buffer Object
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, bufferData.size, bufferData.data, bufferData.usage);

    // Unbind the Vertex Array Object to avoid accidental modifications
    glBindVertexArray(0);
}

void createVBO(GLuint& VBO, const GLuint& VAO, const BufferData& bufferData, const VertexAttribute& vertexAttrib)
{
    // Bind the VAO
    glBindVertexArray(VAO);

    // Generate and bind the instance Vertex Buffer Object
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, bufferData.size, bufferData.data, bufferData.usage);

    // Set up vertex attributes for instance data
    glVertexAttribPointer(vertexAttrib.index, vertexAttrib.size, vertexAttrib.type, vertexAttrib.normalized, vertexAttrib.stride, vertexAttrib.offset);
    glEnableVertexAttribArray(vertexAttrib.index);
    glVertexAttribDivisor(vertexAttrib.index, 1);

    // Unbind the Vertex Array Object to avoid accidental modifications
    glBindVertexArray(0);
}

void initTransparency(HWND hwnd)
{
    // Enable transparency
    DWM_BLURBEHIND bb = { 0 };
    HRGN hRgn = CreateRectRgn(0, 0, -1, -1);
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.hRgnBlur = hRgn;
    bb.fEnable = TRUE;
    DwmEnableBlurBehindWindow(hwnd, &bb);

    // Enable click through
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT);

    // Set window always on top
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

float rndFloat(float lower, float upper)
{
    float random = ((float)rand()) / RAND_MAX;
    return lower + random * (upper - lower);
}

bool rndBool()
{
    return rand() & 1;
}

double getDeltaTima(double& lastFrameTime)
{
    double currentFrameTime = glfwGetTime();
    double deltaTime = currentFrameTime - lastFrameTime;
    lastFrameTime = currentFrameTime;
    return deltaTime;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        if (WIREFRAME_ENABLED)
        {
            WIREFRAME_ENABLED = !WIREFRAME_ENABLED;
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
        else
        {
            WIREFRAME_ENABLED = !WIREFRAME_ENABLED;
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
    }
}

int main()
{
    glfwInit();
    //glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "OpenGL", NULL, NULL);
    glfwSetKeyCallback(window, keyCallback);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    //initTransparency(glfwGetWin32Window(window));
    glewInit();

    float vertices[] = {
        1.0f,  1.0f,    // top right
        1.0f, -1.0f,    // bottom right
        -1.0f, -1.0f,   // bottom left
        -1.0f,  1.0f    // top left
    };
    unsigned int indices[] = {
        0, 1, 3,   // first triangle
        1, 2, 3    // second triangle
    };
    
    const int instances_index = 100;
    InstanceData* instances = new InstanceData[instances_index];

    for (size_t i = 0; i < instances_index; i++)
    {
        glm::vec2 position(rndFloat(-ASPECT_RATIO, ASPECT_RATIO), rndFloat(-1.0f, 1.0f));
        glm::vec3 color(rndFloat(0.0f, 1.0f), rndFloat(0.0f, 1.0f), rndFloat(0.0f, 1.0f));
        glm::vec2 scale = (rndBool()) ? glm::vec2(0.025f, 0.0f) : glm::vec2(0.025f, 0.025f);
        float angle = 0.0f;

        instances[i] = {
            position,
            scale,
            color,
            angle
        };
    }

    GLuint VAO, VBO, EBO, iPosVBO, iColorVBO, iModelVBO, iAngleVBO;
    
    // Create VAO and VBO for vertices and indices
    VertexAttribute vertexAttr = { 0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0 };
    BufferData vertexData = { vertices, sizeof(vertices), GL_STATIC_DRAW };
    setupBuffers(VAO, VBO, vertexData, vertexAttr);

    // Create indices buffer
    BufferData indexData = { indices, sizeof(indices), GL_STATIC_DRAW };
    createEBO(EBO, VAO, indexData);

    // Create VBO for instance positions
    VertexAttribute instanceAttr = { 1, 2, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, position) };
    BufferData instanceData = { instances, instances_index * sizeof(InstanceData), GL_STATIC_DRAW };
    createVBO(iPosVBO, VAO, instanceData, instanceAttr);

    // Create VBO for instance colors
    VertexAttribute colorAttr = { 2, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, color) };
    BufferData colorData = { instances, instances_index * sizeof(InstanceData), GL_STATIC_DRAW};
    createVBO(iColorVBO, VAO, colorData, colorAttr);

    // Create VBO for instance models
    VertexAttribute scaleAttr = { 3, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, scale) };
    BufferData scaleData = { instances, instances_index * sizeof(InstanceData), GL_STATIC_DRAW };
    createVBO(iModelVBO, VAO, scaleData, scaleAttr);

    // Create VBO for instance angles
    VertexAttribute angleAttr = { 4, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, angle) };
    BufferData angleData = { instances, instances_index * sizeof(InstanceData), GL_STATIC_DRAW };
    createVBO(iAngleVBO, VAO, angleData, angleAttr);

    glm::mat4 orthoMatrix = glm::ortho(-ASPECT_RATIO, ASPECT_RATIO, -1.0f, 1.0f, -1.0f, 1.0f);

    GLuint shader = compileShader((char*)vertexCircleSource, (char*)fragmentCircleSource);
    
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(orthoMatrix));
    glUniform2f(glGetUniformLocation(shader, "resolution"), WINDOW_WIDTH, WINDOW_HEIGHT);

    float rotationAngle = 0.0f;             // Initial rotation angle
    float rotationSpeed = 1.0f;             // Adjust as needed (radians per second)
    double lastFrameTime = glfwGetTime();   // Get initial time

    while (!glfwWindowShouldClose(window))
    {
        {
            rotationAngle += (rotationSpeed * (float)getDeltaTima(lastFrameTime));
            if (rotationAngle >= 2.0f * glm::pi<float>()) rotationAngle -= 2.0f * glm::pi<float>();
            for (size_t i = 0; i < instances_index; i++)
            {
                instances[i].angle = rotationAngle;
                instances[i].position = glm::vec2(rndFloat(-ASPECT_RATIO, ASPECT_RATIO), rndFloat(-1.0f, 1.0f));
            }
            glBindBuffer(GL_ARRAY_BUFFER, iAngleVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, instances_index * sizeof(InstanceData), instances);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glBindBuffer(GL_ARRAY_BUFFER, iPosVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, instances_index * sizeof(InstanceData), instances);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader);
        glBindVertexArray(VAO);
        glDrawElementsInstanced(GL_TRIANGLES, sizeof(indices) / sizeof(float), GL_UNSIGNED_INT, 0, instances_index);
        glBindVertexArray(0);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
