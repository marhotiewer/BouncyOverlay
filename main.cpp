#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <dwmapi.h>
#include <GL/glew.h>
#include <GL/wglew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const int WINDOW_WIDTH = 2560;
const int WINDOW_HEIGHT = 1440;
const float ASPECT_RATIO = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;

const char* vertexShaderSource = R"(
    #version 460 core

    uniform mat4 projectionMatrix; // Projection matrix uniform

    layout (location = 0) in vec2 aPos;

    void main()
    {
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

HWND initTransparency(SDL_Window* window) {
    SDL_SysWMinfo wmInfo{ 0 };
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

    // Enable click through
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT);

    // Enable transparency
    DWM_BLURBEHIND bb = { 0 };
    HRGN hRgn = CreateRectRgn(0, 0, -1, -1);
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.hRgnBlur = hRgn;
    bb.fEnable = TRUE;
    DwmEnableBlurBehindWindow(hwnd, &bb);

    return hwnd;
}

HDC initOpenGL(HWND hwnd) {
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,                                // Version Number
        PFD_DRAW_TO_WINDOW |              // Format Must Support Window
        PFD_SUPPORT_OPENGL |              // Format Must Support OpenGL
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

    HDC hdc = GetDC(hwnd);
    INT pixelFormat = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pixelFormat, &pfd);
    HGLRC hrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hrc);
    glewInit();

    return hdc;
}

GLuint initShaders()
{
    // Compile shaders
    GLuint vertexShader, fragmentShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);

    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);

    // Link shaders into a shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Delete shaders after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
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
    float vertices[101][2];
    GLuint VAO, VBO;
    float norm_radius;
    vec2 delta;
public:
    vec2 position;
    float radius;
    Circle(float radius, vec2 pos, vec2 delta) : radius(radius), position(pos), delta(delta), norm_radius(radius / WINDOW_HEIGHT * 2) {
        generateVertices();
        setupBuffers();
    }

    void generateVertices() {
        vertices[0][0] = position.get_x_norm();
        vertices[0][1] = position.get_y_norm();
        for (int i = 1; i <= 100; i++) {
            float theta = 2.0f * float(M_PI) * float(i - 1) / float(99);
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
        glDrawArrays(GL_TRIANGLE_FAN, 0, 101);
        glBindVertexArray(0);
    }

    void move(vec2 delta) {
        position += delta;
        generateVertices();
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void update() {
        if (position.x > WINDOW_WIDTH - radius)
        {
            delta.x = -delta.x;
        }
        if (position.x < radius)
        {
            delta.x = -delta.x;
        }
        if (position.y > WINDOW_HEIGHT - radius)
        {
            delta.y = -delta.y;
        }
        if (position.y < radius)
        {
            delta.y = -delta.y;
        }
        move(delta);
    }
};

int main(int argc, char* argv[])
{
    SDL_Window* window          = SDL_CreateWindow("OpenGL", 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_SKIP_TASKBAR);
    HWND        hwnd            = initTransparency(window);
    HDC         hdc             = initOpenGL(hwnd);
    GLuint      shaderProgram   = initShaders();

    // Set up orthographic view, we only do this once because the view wont get changed
    GLint projectionMatrixLocation = glGetUniformLocation(shaderProgram, "projectionMatrix");
    glm::mat4 orthoMatrix = glm::ortho(-ASPECT_RATIO, ASPECT_RATIO, -1.0f, 1.0f, -1.0f, 1.0f);

    Circle circle1(50, vec2(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2), vec2(5, 5));
    Circle circle2(50, vec2(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2), vec2(-5, -5));
    Circle circle3(50, vec2(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2), vec2(5, -5));
    Circle circle4(50, vec2(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2), vec2(-5, 5));
    SDL_Event windowEvent;
    while (true)
    {
        if (SDL_PollEvent(&windowEvent))
        {
            if (windowEvent.type == SDL_QUIT) break;
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, glm::value_ptr(orthoMatrix));
        circle1.render();
        circle2.render();
        circle3.render();
        circle4.render();
        glFlush();
        SwapBuffers(hdc);
        circle1.update();
        circle2.update();
        circle3.update();
        circle4.update();
    }
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(wglGetCurrentContext());
    ReleaseDC(hwnd, hdc);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
