#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <cstdio>

const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1080;
const float ASPECT_RATIO = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
constexpr int circleSegments = 100;

GLfloat projectionMatrixData[16];
GLint projectionMatrixLocation; 
GLuint shaderProgram;

struct Circle {
private:
    float vertices[circleSegments + 1][2];
    GLuint VAO, VBO;
public:
    float radius;
    float X, Y;
    Circle(float radius, float positionX, float positionY) : radius(radius), X(positionX), Y(positionY) {
        generateVertices();
        setupBuffers();
    }

    void generateVertices() {
        vertices[0][0] = X;
        vertices[0][1] = Y;
        for (int i = 1; i <= circleSegments; i++) {
            float theta = 2.0f * float(M_PI) * float(i - 1) / float(circleSegments - 1);
            vertices[i][0] = X + radius * (float)cos(theta);
            vertices[i][1] = Y + radius * (float)sin(theta);
        }
    }

    void setupBuffers() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void render() {
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLE_FAN, 0, circleSegments + 1);
        glBindVertexArray(0);
    }

    float delta = 0.01f;
    void update() {

        if (X > ASPECT_RATIO - radius)
        {
            delta = -0.01f;
        }
        if (X < -ASPECT_RATIO + radius)
        {
            delta = 0.01f;
        }

        X += delta;
        generateVertices();
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
};

const char* vertexShaderSource = R"(
    #version 460 core

    uniform mat4 projectionMatrix; // Projection matrix uniform

    layout (location = 0) in vec2 aPos;

    void main()
    {
        // Apply the projection matrix transformation
        gl_Position = projectionMatrix * vec4(aPos, 0.0, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 460 core
    out vec4 FragColor;
    
    void main()
    {
        FragColor = vec4(1.0, 0.5, 0.8, 1.0); // Bright pink color
    }
)";

void generateOrthographicProjection(float left, float right, float bottom, float top, float nearVal, float farVal, GLfloat* projectionMatrix) {
    projectionMatrix[0] = 2.0f / (right - left);
    projectionMatrix[5] = 2.0f / (top - bottom);
    projectionMatrix[10] = -2.0f / (farVal - nearVal);
    projectionMatrix[12] = -(right + left) / (right - left);
    projectionMatrix[13] = -(top + bottom) / (top - bottom);
    projectionMatrix[14] = -(farVal + nearVal) / (farVal - nearVal);
    projectionMatrix[15] = 1.0f;
}

void initGL()
{
    glewInit();

    // Compile shaders
    GLuint vertexShader, fragmentShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);

    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);

    // Link shaders into a shader program
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Delete shaders after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    projectionMatrixLocation = glGetUniformLocation(shaderProgram, "projectionMatrix");
    generateOrthographicProjection(-ASPECT_RATIO, ASPECT_RATIO, -1.0f, 1.0f, -1.0f, 1.0f, projectionMatrixData);
}

int main(int argc, char* argv[])
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_Window* window = SDL_CreateWindow("OpenGL", 100, 100, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    initGL();

    Circle circle1(0.25f, 0.0f, 0.0f);
    Circle circle2(0.25f, -0.5f, -0.5f);
    Circle circle3(0.25f, 0.5f, 0.5f);
    Circle circle4(0.25f, -0.5f, 0.5f);
    Circle circle5(0.25f, 0.5f, -0.5f);

    SDL_Event windowEvent;
    while (true)
    {
        if (SDL_PollEvent(&windowEvent))
        {
            if (windowEvent.type == SDL_QUIT) break;
        }
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, projectionMatrixData);

        circle1.update();
        circle2.update();
        circle3.update();
        circle4.update();
        circle5.update();

        circle1.render();
        circle2.render();
        circle3.render();
        circle4.render();
        circle5.render();

        SDL_GL_SwapWindow(window);
    }
    SDL_GL_DeleteContext(context);
    SDL_Quit();
    return 0;
}
