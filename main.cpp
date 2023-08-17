#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <dwmapi.h>
#include <GL/glew.h>
#include <GL/wglew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <box2d/box2d.h>

const int WINDOW_WIDTH = GetSystemMetrics(SM_CXSCREEN) - 1;
const int WINDOW_HEIGHT = GetSystemMetrics(SM_CYSCREEN) - 1;
const float ASPECT_RATIO = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
GLint vertexColorLocation;

const char* vertexShaderSource = R"(
    #version 460 core

    uniform mat4 projectionMatrix; // Projection matrix uniform
    uniform vec4 vertexColor;      // Uniform color for all vertices

    layout (location = 0) in vec2 aPos;

    out vec4 fragColor; // Output color to fragment shader

    void main()
    {
        gl_Position = projectionMatrix * vec4(aPos, 0.0, 1.0);
        fragColor = vertexColor; // Pass the uniform color to the fragment shader
    }
)";

const char* fragmentShaderSource = R"(
    #version 460 core
    in vec4 fragColor; // Input color from vertex shader
    out vec4 FragColor;
    
    void main()
    {
        FragColor = fragColor; // Use the input color as the fragment color
    }
)";

HWND initTransparency(SDL_Window* window)
{
    // Get HWND handle from SDL_Window
    SDL_SysWMinfo wmInfo{ 0 };
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

    // Enable transparency
    DWM_BLURBEHIND bb = { 0 };
    HRGN hRgn = CreateRectRgn(0, 0, -1, -1);
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.hRgnBlur = hRgn;
    bb.fEnable = TRUE;
    DwmEnableBlurBehindWindow(hwnd, &bb);

    // Enable click through
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT);

    // Hide window from task bar and switcher
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_NOACTIVATE);

    // Set window always on top
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    return hwnd;
}

HDC initOpenGL(HWND hwnd)
{
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

int randomNum(int lower, int upper)
{
    return lower + rand() % (upper - lower + 1);
}

struct Circle
{
    Circle(glm::vec3 color, int radius, glm::vec2 pos, b2World& world) : radius(radius), position(pos) {
        generateVertices();
        setupBuffers();
        setupPhysics(world);

        normColor.r = color.r / 255;
        normColor.g = color.g / 255;
        normColor.b = color.b / 255;
    }
    void generateVertices() {
        float normRadius = (float)radius / WINDOW_HEIGHT * 2;
        float normX = -ASPECT_RATIO + (2.0f * position.x / WINDOW_WIDTH) * ASPECT_RATIO;
        float normY = 1.0f - (2.0f * position.y / WINDOW_HEIGHT);

        vertices[0][0] = normX;
        vertices[0][1] = normY;
        for (int i = 1; i <= segments; i++) {
            float theta = 2.0f * float(M_PI) * float(i - 1) / float(segments - 1);
            vertices[i][0] = normX + normRadius * (float)cos(theta);
            vertices[i][1] = normY + normRadius * (float)sin(theta);
        }
    }
    void setupPhysics(b2World& world) {
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.position.Set(position.x / 48.0f, position.y / 48.0f);

        b2CircleShape circle;
        circle.m_radius = radius / 48.0f;

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &circle;
        fixtureDef.density = 1.0f;
        fixtureDef.friction = 1.0f;
        fixtureDef.restitution = 0.75f;

        body = world.CreateBody(&bodyDef);
        body->CreateFixture(&fixtureDef);
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
    void applyForce(b2Vec2 force) {
        body->ApplyForce(force, body->GetPosition(), true);
    }
    void render() {
        glUniform4f(vertexColorLocation, normColor.r, normColor.g, normColor.b, 1.0f); // Set the desired color here
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLE_FAN, 0, segments + 1);
        glBindVertexArray(0);
    }
    void update() {
        b2Vec2 pos = body->GetPosition();
        position.x = pos.x * 48.0f;
        position.y = pos.y * 48.0f;
        generateVertices();
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
private:
    const int segments = 100;
    float vertices[100+1][2];
    glm::vec3 normColor;
    glm::vec2 position;
    GLuint VAO, VBO;
    b2Body* body;
    int radius;
};

struct Wall
{
    b2Body* body;
    Wall(glm::vec2 pos, glm::vec2 size, b2World& world) {
        b2BodyDef groundBodyDef;
        groundBodyDef.position.Set(pos.x / 48.0f, pos.y / 48.0f);

        b2PolygonShape groundBox;
        groundBox.SetAsBox(0.5f * size.x / 48.0f, 0.5f * size.y / 48.0f);

        body = world.CreateBody(&groundBodyDef);
        body->CreateFixture(&groundBox, 0.0f);
    }
};

int main(int argc, char* argv[])
{
    SDL_Window* window          = SDL_CreateWindow("OpenGL", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_BORDERLESS);
    HWND        hwnd            = initTransparency(window);
    HDC         hdc             = initOpenGL(hwnd);
    GLuint      shaderProgram   = initShaders();
    
    // Set up orthographic view, we only do this once because the view wont get changed
    GLint projectionMatrixLocation = glGetUniformLocation(shaderProgram, "projectionMatrix");
    vertexColorLocation = glGetUniformLocation(shaderProgram, "vertexColor");
    glm::mat4 orthoMatrix = glm::ortho(-ASPECT_RATIO, ASPECT_RATIO, -1.0f, 1.0f, -1.0f, 1.0f);

    b2World world({ 0.0f, 0.0f });

    Wall(glm::vec2(WINDOW_WIDTH / 2, WINDOW_HEIGHT + 5), glm::vec2(WINDOW_WIDTH, 10), world);
    Wall(glm::vec2(WINDOW_WIDTH / 2, -5), glm::vec2(WINDOW_WIDTH, 10), world);
    Wall(glm::vec2(-5, WINDOW_HEIGHT / 2), glm::vec2(10, WINDOW_HEIGHT), world);
    Wall(glm::vec2(WINDOW_WIDTH + 5, WINDOW_HEIGHT / 2), glm::vec2(10, WINDOW_HEIGHT), world);

    const int circles_size = 1000;
    Circle* circles[circles_size] { 0 };
    size_t position = 0;

    SDL_Event windowEvent;
    float timePassed = 0.0f;
    Uint32 prevTicks = SDL_GetTicks();

    while (true)
    {
        Uint32 currentTicks = SDL_GetTicks();
        float deltaTime = (currentTicks - prevTicks) / 1000.0f; // deltaTime in seconds
        prevTicks = currentTicks;
        timePassed += deltaTime;
        
        if (SDL_PollEvent(&windowEvent))
        {
            if (windowEvent.type == SDL_QUIT) break;
        }
        if (deltaTime < 1.0f / 60) {
            world.Step(deltaTime, 6, 2);
        }
        if (position < circles_size && timePassed > 0.01f)
        {
            glm::vec2   randomPosition(randomNum(50, WINDOW_WIDTH - 50), randomNum(50, WINDOW_HEIGHT - 50));
            glm::vec3   randomColor(randomNum(0, 255), randomNum(0, 255), randomNum(0, 255));
            int         randomRadius = randomNum(5, 25);
            b2Vec2      randomForce((float)randomNum(-1000, 1000), (float)randomNum(-1000, 1000));

            circles[position] = new Circle(randomColor, randomRadius, randomPosition, world);
            circles[position]->applyForce(randomForce);
            
            timePassed = 0.0f;
            position++;
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, glm::value_ptr(orthoMatrix));
        for (size_t i = 0; i < position; i++)
        {
            circles[i]->update();
            circles[i]->render();
        }
        glFlush();
        SwapBuffers(hdc);
    }
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(wglGetCurrentContext());
    ReleaseDC(hwnd, hdc);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
