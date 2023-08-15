#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <GL/glew.h>
#include <cstdio>
#include <windows.h>
#include <dwmapi.h>

const int WINDOW_WIDTH = 2560;
const int WINDOW_HEIGHT = 1440;
const float ASPECT_RATIO = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
constexpr int circleSegments = 100;

GLfloat projectionMatrixData[16];
GLint projectionMatrixLocation; 
GLuint shaderProgram;

HDC initTransparency(SDL_Window* window) {
    SDL_SysWMinfo wmInfo{ 0 };
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

    SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT);
    SetLayeredWindowAttributes(hwnd, 0, 0, 0);

    DWM_BLURBEHIND bb = { 0 };
    HRGN hRgn = CreateRectRgn(0, 0, -1, -1);
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.hRgnBlur = hRgn;
    bb.fEnable = TRUE;
    DwmEnableBlurBehindWindow(hwnd, &bb);

    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,                                // Version Number
        PFD_DRAW_TO_WINDOW |              // Format Must Support Window
        PFD_SUPPORT_OPENGL |              // Format Must Support OpenGL
        PFD_SUPPORT_COMPOSITION |         // Format Must Support Composition
        PFD_DOUBLEBUFFER,                 // Must Support Double Buffering
        PFD_TYPE_RGBA,                    // Request An RGBA Format
        32,                               // Select Our Color Depth
        0, 0, 0, 0, 0, 0,                 // Color Bits Ignored
        8,                                // An Alpha Buffer
        0,                                // Shift Bit Ignored
        0,                                // No Accumulation Buffer
        0, 0, 0, 0,                       // Accumulation Bits Ignored
        24,                               // 16Bit Z-Buffer (Depth Buffer)
        8,                                // Some Stencil Buffer
        0,                                // No Auxiliary Buffer
        PFD_MAIN_PLANE,                   // Main Drawing Layer
        0,                                // Reserved
        0, 0, 0                           // Layer Masks Ignored
    };

    HDC hDC = GetDC(hwnd);
    INT pixelFormat = ChoosePixelFormat(hDC, &pfd);
    SetPixelFormat(hDC, pixelFormat, &pfd);
    HGLRC hRC = wglCreateContext(hDC);
    wglMakeCurrent(hDC, hRC);

    return hDC;
}

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

void generateOrthographicProjection(float left, float right, float bottom, float top, float nearVal, float farVal, GLfloat* projectionMatrix)
{
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

struct vec2 {
    int x, y;
    vec2(int x, int y) : x(x), y(y) {}
    float get_x_norm() {
        return -ASPECT_RATIO + (2.0f * x / WINDOW_WIDTH) * ASPECT_RATIO;
    }
    float get_y_norm() {
        return 1.0f - (2.0f * y / WINDOW_HEIGHT);
    }
    vec2 operator+(const vec2& other) const {
        return vec2(x + other.x, y + other.y);
    }
    vec2& operator+=(const vec2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }
};

struct Circle {
private:
    float vertices[circleSegments + 1][2];
    GLuint VAO, VBO;
    float norm_radius;
public:
    vec2 position;
    Circle(float radius, vec2 pos) : norm_radius(radius / WINDOW_HEIGHT * 2), position(pos) {
        generateVertices();
        setupBuffers();
    }

    void generateVertices() {
        vertices[0][0] = position.get_x_norm();
        vertices[0][1] = position.get_y_norm();
        for (int i = 1; i <= circleSegments; i++) {
            float theta = 2.0f * float(M_PI) * float(i - 1) / float(circleSegments - 1);
            vertices[i][0] = position.get_x_norm() + norm_radius * (float)cos(theta);
            vertices[i][1] = position.get_y_norm() + norm_radius * (float)sin(theta);
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

    void move(vec2 delta) {
        position += delta;
        generateVertices();
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
};

int main(int argc, char* argv[])
{
    SDL_Window* window = SDL_CreateWindow("OpenGL", 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALWAYS_ON_TOP);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    
    HDC hDC = initTransparency(window);
    initGL();

    Circle circle1(25, vec2(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
    vec2 delta(10, 10);

    SDL_Event windowEvent;
    while (true)
    {
        if (SDL_PollEvent(&windowEvent))
        {
            if (windowEvent.type == SDL_QUIT) break;
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, projectionMatrixData);
        circle1.render();
        glFlush();
        SwapBuffers(hDC);

        if (circle1.position.x > WINDOW_WIDTH - 25)
        {
            delta.x = -10;
        }
        if (circle1.position.x < 25)
        {
            delta.x = 10;
        }
        if (circle1.position.y > WINDOW_HEIGHT - 25)
        {
            delta.y = -10;
        }
        if (circle1.position.y < 25)
        {
            delta.y = 10;
        }
        circle1.move(delta);
    }
    SDL_GL_DeleteContext(context);
    SDL_Quit();
    return 0;
}
