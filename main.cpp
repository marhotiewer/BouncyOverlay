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

const char* vertexShaderSource = R"(
    #version 330 core

    uniform mat4 projection;                // projection matrix uniform

    layout (location = 0) in vec2 inPos;     // original position

    layout (location = 1) in vec2 iPos;     // instanced position
    layout (location = 2) in vec3 iColor;   // instanced color
    layout (location = 3) in vec3 iModel;   // instanced color

    out vec3 vColorOut; 

    mat2 rotationMatrix(float angle) {
        float s = sin(angle);
        float c = cos(angle);
        return mat2(c, -s, s, c);
    }

    void main()
    {
        vec2 scale = vec2(iModel.x, iModel.y);
        float radians = iModel.z;

        vec2 newPos = rotationMatrix(radians) * (inPos * scale);
        gl_Position = projection * vec4(newPos + iPos, 0.0, 1.0);
        vColorOut = iColor;
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core

    in vec3 vColorOut;
    out vec4 FragColor;

    void main()
    {
        FragColor = vec4(vColorOut, 1.0f);
    }
)";

const int WINDOW_WIDTH = 2560;
const int WINDOW_HEIGHT = 1440+1;
const float ASPECT_RATIO = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
bool WIREFRAME_ENABLED = false;

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

void setupBuffers(GLuint& VAO, GLuint& VBO, GLuint& iVBO, GLuint& EBO,
    const BufferData& vertexBufferData,
    const BufferData& indexBufferData,
    const VertexAttribute& vertexAttribute)
{
    // Generate and bind the Vertex Array Object
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Generate and bind the Vertex Buffer Object
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexBufferData.size, vertexBufferData.data, vertexBufferData.usage);

    // Generate and bind the Index Buffer Object
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferData.size, indexBufferData.data, indexBufferData.usage);

    // Set up vertex attributes for the main vertex data
    glVertexAttribPointer(vertexAttribute.index, vertexAttribute.size, vertexAttribute.type, vertexAttribute.normalized, vertexAttribute.stride, vertexAttribute.offset);
    glEnableVertexAttribArray(vertexAttribute.index);

    // Unbind the Vertex Array Object to avoid accidental modifications
    glBindVertexArray(0);
}

GLuint createVBO(GLuint& VBO, const GLuint& VAO, const BufferData& bufferData, const VertexAttribute& vertexAttrib)
{
    glBindVertexArray(VAO);

    // Generate and bind the instance Vertex Buffer Object
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, bufferData.size, bufferData.data, bufferData.usage);

    // Set up vertex attributes for instance data
    glVertexAttribPointer(vertexAttrib.index, vertexAttrib.size, vertexAttrib.type, vertexAttrib.normalized, vertexAttrib.stride, vertexAttrib.offset);
    glEnableVertexAttribArray(vertexAttrib.index);
    glVertexAttribDivisor(vertexAttrib.index, 1);

    glBindVertexArray(0);
    return VBO;
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

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
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
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE); // This makes the window borderless
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "OpenGL", NULL, NULL);
    glfwSetKeyCallback(window, keyCallback);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    glewInit();

    HWND hwnd = glfwGetWin32Window(window);
    initTransparency(hwnd);

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
    
    const int instances_index = 1000;
    glm::vec2* instances = new glm::vec2[instances_index];
    glm::vec3* instance_colors = new glm::vec3[instances_index];
    glm::vec3* instance_models = new glm::vec3[instances_index];

    for (size_t i = 0; i < instances_index; i++)
    {
        glm::vec2 rndPos(rndFloat(-ASPECT_RATIO, ASPECT_RATIO), rndFloat(-1.0f, 1.0f));
        glm::vec3 rndModel(0.01f, 0.01f, rndFloat(0.0f, 1.0f));
        glm::vec3 rndCol(rndFloat(0.0f, 1.0f), rndFloat(0.0f, 1.0f), rndFloat(0.0f, 1.0f));
        instances[i] = rndPos;
        instance_colors[i] = rndCol;
        instance_models[i] = rndModel;
    }

    GLuint VAO, VBO, EBO, iPosVBO, iColorVBO, iModelVBO;
    
    // Create VAO and two VBOs for vertices and indices
    VertexAttribute vertexAttr = { 0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0 };
    BufferData vertexData = { vertices, sizeof(vertices), GL_STATIC_DRAW };
    BufferData indexData = { indices, sizeof(indices), GL_STATIC_DRAW };
    setupBuffers(VAO, VBO, iPosVBO, EBO, vertexData, indexData, vertexAttr);

    // Create VBO for instance positions
    VertexAttribute instanceAttr = { 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0 };
    BufferData instanceData = { instances, instances_index * (2 * sizeof(float)), GL_STATIC_DRAW };
    createVBO(iPosVBO, VAO, instanceData, instanceAttr);

    // Create VBO for instance colors
    VertexAttribute colorAttr = { 2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0 };
    BufferData colorData = { instance_colors, instances_index * (3 * sizeof(float)), GL_STATIC_DRAW};
    createVBO(iColorVBO, VAO, colorData, colorAttr);

    // Create VBO for instance models
    VertexAttribute scaleAttr = { 3, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0 };
    BufferData scaleData = { instance_models, instances_index * (3 * sizeof(float)), GL_STATIC_DRAW };
    createVBO(iModelVBO, VAO, scaleData, scaleAttr);

    GLuint shaderProgram = compileShader((char*)vertexShaderSource, (char*)fragmentShaderSource);
    glUseProgram(shaderProgram);
    glm::mat4 orthoMatrix = glm::ortho(-ASPECT_RATIO, ASPECT_RATIO, -1.0f, 1.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(orthoMatrix));

    while (!glfwWindowShouldClose(window))
    {
        {
            for (size_t i = 0; i < instances_index; i++)
            {
                glm::vec2 rndPos(rndFloat(-ASPECT_RATIO, ASPECT_RATIO), rndFloat(-1.0f, 1.0f));
                glm::vec3 rndModel(0.01f, 0.01f, rndFloat(0.0f, 1.0f));
                glm::vec3 rndCol(rndFloat(0.0f, 1.0f), rndFloat(0.0f, 1.0f), rndFloat(0.0f, 1.0f));
                instances[i] = rndPos;
                instance_colors[i] = rndCol;
                instance_models[i] = rndModel;
            }
            glBindBuffer(GL_ARRAY_BUFFER, iPosVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, instances_index * (2 * sizeof(float)), instances);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glBindBuffer(GL_ARRAY_BUFFER, iColorVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, instances_index * (3 * sizeof(float)), instance_colors);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glBindBuffer(GL_ARRAY_BUFFER, iModelVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, instances_index * (3 * sizeof(float)), instance_models);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawElementsInstanced(GL_TRIANGLES, sizeof(indices) / sizeof(float), GL_UNSIGNED_INT, 0, instances_index);
        glBindVertexArray(0);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
