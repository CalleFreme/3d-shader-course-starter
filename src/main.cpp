#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

// This is our main file for the OpenGL application.
// It sets up a window, compiles shaders, and renders a lit, textured cube.
// 
// What work will be done in this file:
// 1. Initialize GLFW and create a window.
// 2. Load OpenGL functions using GLAD.
// 3. Compile vertex and fragment shaders from external files.
// 4. Set up vertex data, buffers, and a small generated texture.
// 5. Render the cube into a texture, then draw that texture on a fullscreen quad.
// 
// What work will be done in other files:
// 1. basic.vert/basic.frag shade the cube; screen.vert/screen.frag display its image.
// 2. Each vertex/fragment pair is compiled and linked into its own program.
// 3. The CPU selects the program and framebuffer before each draw.
// 4. The vertex data will be stored in a vertex buffer object (VBO) and a
//    vertex array object (VAO).
// 5. The cube will be rendered using glDrawArrays with the shader program
//	and the vertex data.
// 6. The application will handle window resizing and input events.
// 7. The application will clean up resources and terminate GLFW on exit.
// 
// Note: This code is based on the OpenGL 3.3 core profile and uses modern OpenGL
// techniques. It does not use deprecated functions or fixed-function pipeline features.

namespace
{
constexpr int WindowWidth = 900;
constexpr int WindowHeight = 600;

void glfwErrorCallback(int error, const char* description)
{
    std::cerr << "GLFW error (" << error << "): " << description << '\n';
}

std::string readTextFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
    {
        throw std::runtime_error("Could not open file: " + path);
    }

    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

GLuint compileShader(GLenum type, const std::string& source, const std::string& label)
{
    const GLuint shader = glCreateShader(type);
    const char* sourcePtr = source.c_str();

    glShaderSource(shader, 1, &sourcePtr, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (success == GL_FALSE)
    {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

        std::string log(static_cast<std::size_t>(logLength), '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());

        glDeleteShader(shader);
        throw std::runtime_error("Shader compilation failed (" + label + "):\n" + log);
    }

    return shader;
}

GLuint createShaderProgram(const std::string& vertexPath, const std::string& fragmentPath)
{
    const std::string vertexSource = readTextFile(vertexPath);
    const std::string fragmentSource = readTextFile(fragmentPath);

    const GLuint vertexShader =
        compileShader(GL_VERTEX_SHADER, vertexSource, vertexPath);
    GLuint fragmentShader = 0;
    try
    {
        fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, fragmentPath);
    }
    catch (...)
    {
        glDeleteShader(vertexShader);
        throw;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (success == GL_FALSE)
    {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

        std::string log(static_cast<std::size_t>(logLength), '\0');
        glGetProgramInfoLog(program, logLength, nullptr, log.data());

        glDeleteProgram(program);
        throw std::runtime_error("Shader program link failed:\n" + log);
    }

    return program;
}

// Called only for positive dimensions, on first use and whenever the size changes.
bool resizeSceneFramebuffer(GLuint framebuffer, GLuint colorTexture,
                            GLuint depthStencil, int width, int height)
{
    // nullptr allocates image storage without uploading CPU pixels. The GPU
    // will fill this texture when we clear and draw the scene in pass 1.
    // Reallocating storage keeps the object names and their attachments intact.
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    // Keep depth storage the same size as colour: both describe the same image.
    // This packed format reserves 24 bits for depth and 8 for stencil;
    // only depth testing is used in this baseline.
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    // Check after storage exists, including after a resize. Completeness means
    // the attachments form a usable render target, not that the scene is correct.
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Scene framebuffer incomplete at " << width << 'x' << height
                  << ": status 0x" << std::hex << status << std::dec << '\n';
        return false;
    }
    return true;
}

enum class ScreenMode
{
    Normal,
    Workload,
    Discard
};

void processInput(GLFWwindow* window, ScreenMode& mode, int& iterationCount,
                  int& discardOrder, bool& vsyncEnabled)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    // Edge-triggered keys keep the tiny demonstration controls easy to trace.
    static int previousOne = GLFW_RELEASE;
    static int previousTwo = GLFW_RELEASE;
    static int previousThree = GLFW_RELEASE;
    static int previousDown = GLFW_RELEASE;
    static int previousUp = GLFW_RELEASE;
    static int previousD = GLFW_RELEASE;
    static int previousV = GLFW_RELEASE;
    const auto pressed = [](GLFWwindow* currentWindow, int key, int& previous)
    {
        const int current = glfwGetKey(currentWindow, key);
        const bool wasPressed = current == GLFW_PRESS && previous == GLFW_RELEASE;
        previous = current;
        return wasPressed;
    };

    if (pressed(window, GLFW_KEY_1, previousOne))
    {
        mode = ScreenMode::Normal;
        std::cout << "Screen mode: NORMAL\n";
    }
    if (pressed(window, GLFW_KEY_2, previousTwo))
    {
        mode = ScreenMode::Workload;
        std::cout << "Screen mode: WORKLOAD DEMO (iterations: " << iterationCount
                  << ", VSync: " << (vsyncEnabled ? "ON" : "OFF") << ")\n";
    }
    if (pressed(window, GLFW_KEY_3, previousThree))
    {
        mode = ScreenMode::Discard;
        std::cout << "Screen mode: DISCARD DEMO (iterations: " << iterationCount
                  << ", " << (discardOrder == 0 ? "work before discard" : "discard before work")
                  << ", VSync: " << (vsyncEnabled ? "ON" : "OFF") << ")\n";
    }
    // The shared count is used by both demos, so the same amount of repeated
    // work can be compared before and after changing the discard order.
    if (pressed(window, GLFW_KEY_DOWN, previousDown))
    {
        iterationCount = std::max(1, iterationCount / 2);
        std::cout << "Workload iterations: " << iterationCount << "\n";
    }
    if (pressed(window, GLFW_KEY_UP, previousUp))
    {
        iterationCount = std::min(1024, iterationCount * 2);
        std::cout << "Workload iterations: " << iterationCount << "\n";
    }
    if (pressed(window, GLFW_KEY_D, previousD))
    {
        discardOrder = 1 - discardOrder;
        std::cout << "Discard order: " << (discardOrder == 0 ? "work before discard" : "discard before work") << "\n";
    }
    if (pressed(window, GLFW_KEY_V, previousV))
    {
        vsyncEnabled = !vsyncEnabled;
        glfwSwapInterval(vsyncEnabled ? 1 : 0);
        std::cout << "VSync: " << (vsyncEnabled ? "ON" : "OFF") << "\n";
    }
}
} // namespace

int main()
{
	glfwSetErrorCallback(glfwErrorCallback);
	// Need to initialize GLFW before calling any GLFW functions
    if (glfwInit() != GLFW_TRUE)
    {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWwindow* window =
        glfwCreateWindow(WindowWidth, WindowHeight, "3D and Shader Programming", nullptr, nullptr);

    if (window == nullptr)
    {
        std::cerr << "Failed to create a GLFW window.\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    const int loadedVersion = gladLoadGL(glfwGetProcAddress);
    if (loadedVersion == 0)
    {
        std::cerr << "Failed to load OpenGL functions with GLAD.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n';
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << '\n';

    // position.xyz, normal.xyz, uv.xy
    // Each face has its own vertices so it can have one clear, flat normal.
    // The duplicated vertices also let every face own a full UV square.
    constexpr float vertices[] = {
        // Front (+Z)
        -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   1.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   0.0f, 1.0f,

        // Back (-Z)
         0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   0.0f, 1.0f,

        // Left (-X)
        -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,   1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,   1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,   1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   0.0f, 1.0f,

        // Right (+X)
         0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  0.0f,   0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  0.0f,   1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   1.0f,  0.0f,  0.0f,   1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  0.0f,   0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   1.0f,  0.0f,  0.0f,   1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,   0.0f, 1.0f,

        // Top (+Y)
        -0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,   0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,   1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,   1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,   0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,   1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,   0.0f, 1.0f,

        // Bottom (-Y)
        -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,   0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,   1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,   1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,   0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,   1.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,   0.0f, 1.0f
    };

    GLuint vao = 0;
    GLuint vbo = 0;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    constexpr GLsizei stride = 8 * sizeof(float);

    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    GLuint shaderProgram = 0;
    GLuint screenProgram = 0;
    GLuint workloadProgram = 0;
    GLuint discardProgram = 0;

    try
    {
        shaderProgram =
            createShaderProgram("shaders/basic.vert", "shaders/basic.frag");
        screenProgram =
            createShaderProgram("shaders/screen.vert", "shaders/screen.frag");
        workloadProgram =
            createShaderProgram("shaders/screen.vert", "shaders/workload.frag");
        discardProgram =
            createShaderProgram("shaders/screen.vert", "shaders/discard_order.frag");
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        glDeleteProgram(shaderProgram);
        glDeleteProgram(workloadProgram);
        glDeleteProgram(discardProgram);
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Four rows of four RGBA texels, listed from the texture's bottom row up.
    // The two colours form a checker. Their alternating alpha values are not
    // used by the known-good opaque shader, but remain available as mask data.
    constexpr int TextureWidth = 4;
    constexpr int TextureHeight = 4;
    constexpr unsigned char texturePixels[] = {
        230,  70,  50, 255,    40, 180, 220,  64,   230,  70,  50, 255,    40, 180, 220,  64,
         40, 180, 220,  64,   230,  70,  50, 255,    40, 180, 220,  64,   230,  70,  50, 255,
        230,  70,  50, 255,    40, 180, 220,  64,   230,  70,  50, 255,    40, 180, 220,  64,
         40, 180, 220,  64,   230,  70,  50, 255,    40, 180, 220,  64,   230,  70,  50, 255
    };

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        TextureWidth,
        TextureHeight,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        texturePixels);

    // Separate geometry for pass 2: position.xy in clip space, then uv.xy.
    // Two triangles cover the entire viewport. Unlike the cube, this geometry
    // needs no model/view/projection: screen.vert supplies z = 0 and w = 1.
    // Bottom-left maps to UV (0, 0), top-right to (1, 1), preserving image orientation.
    constexpr float screenVertices[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f
    };
    GLuint screenVao = 0;
    GLuint screenVbo = 0;
    glGenVertexArrays(1, &screenVao);
    glGenBuffers(1, &screenVbo);
    glBindVertexArray(screenVao);
    glBindBuffer(GL_ARRAY_BUFFER, screenVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(screenVertices), screenVertices, GL_STATIC_DRAW);
    constexpr GLsizei screenStride = 4 * sizeof(float);
    // This VAO remembers the quad's layout: location 1 is UV here, whereas
    // location 1 in the cube VAO is a normal. Each matches its own vertex shader.
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, screenStride, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, screenStride,
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // A framebuffer connects attachments; the texture/renderbuffer own the storage.
    // Pass 1 writes colour into a texture because pass 2 must sample it.
    // Depth is used only for visibility in pass 1 and is not sampled in pass 2,
    // so a renderbuffer is sufficient for the depth/stencil attachment.
    GLuint sceneFramebuffer = 0;
    GLuint sceneColorTexture = 0;
    GLuint sceneDepthStencil = 0;
    glGenFramebuffers(1, &sceneFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
    glGenTextures(1, &sceneColorTexture);
    glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
    // Linear filtering can blend neighboring texels. Clamp prevents the opposite
    // edge from repeating when an exercise offsets UVs outside [0, 1].
    // GL_LINEAR minification samples level 0 without requiring mipmaps.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Connect texture level 0 to the scene shader's colour output destination.
    // Attachment setup references the texture; it does not copy its pixels.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, sceneColorTexture, 0);
    glGenRenderbuffers(1, &sceneDepthStencil);
    glBindRenderbuffer(GL_RENDERBUFFER, sceneDepthStencil);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, sceneDepthStencil);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Storage is allocated below using the actual drawable size, with no mipmaps.
    int sceneWidth = 0;
    int sceneHeight = 0;
    int exitCode = 0;
    const GLint sceneTextureLocation = glGetUniformLocation(screenProgram, "sceneTexture");
    const GLint workloadTextureLocation = glGetUniformLocation(workloadProgram, "sceneTexture");
    const GLint iterationCountLocation = glGetUniformLocation(workloadProgram, "iterationCount");
    const GLint discardTextureLocation = glGetUniformLocation(discardProgram, "sceneTexture");
    const GLint discardIterationLocation = glGetUniformLocation(discardProgram, "iterationCount");
    const GLint discardOrderLocation = glGetUniformLocation(discardProgram, "workBeforeDiscard");

    ScreenMode screenMode = ScreenMode::Normal;
    int iterationCount = 8;
    int discardOrder = 0;
    bool vsyncEnabled = true;
    std::cout << "Controls: 1 normal | 2 workload | 3 discard | Up/Down workload | D discard order | V VSync | Esc exit\n";
    std::cout << "VSync: ON | whole-frame timing is approximate, not a direct GPU timer; VSync can mask workload differences. Keep window, camera and scene fixed and change one workload variable.\n";
    auto previousFrameTime = std::chrono::steady_clock::now();
    double frameTimeSumMs = 0.0;
    int measuredFrames = 0;

    // Uniform locations identify the three matrix inputs in the vertex shader.
    // We ask for them once after linking, then use the locations when sending
    // matrix values from the CPU to the GPU before drawing.
    const GLint modelLocation = glGetUniformLocation(shaderProgram, "model");
    const GLint viewLocation = glGetUniformLocation(shaderProgram, "view");
    const GLint projectionLocation = glGetUniformLocation(shaderProgram, "projection");
    const GLint normalMatrixLocation = glGetUniformLocation(shaderProgram, "normalMatrix");
    const GLint lightDirectionLocation = glGetUniformLocation(shaderProgram, "lightDirection");
    const GLint lightColorLocation = glGetUniformLocation(shaderProgram, "lightColor");
    const GLint viewPositionLocation = glGetUniformLocation(shaderProgram, "viewPosition");
    const GLint baseColorLocation = glGetUniformLocation(shaderProgram, "baseColor");
    const GLint ambientStrengthLocation = glGetUniformLocation(shaderProgram, "ambientStrength");
    const GLint specularStrengthLocation = glGetUniformLocation(shaderProgram, "specularStrength");
    const GLint shininessLocation = glGetUniformLocation(shaderProgram, "shininess");
    const GLint surfaceTextureLocation =
        glGetUniformLocation(shaderProgram, "surfaceTexture");
    const GLint timeLocation = glGetUniformLocation(shaderProgram, "time");

    if (modelLocation == -1 ||
        viewLocation == -1 ||
        projectionLocation == -1)
    {
        std::cerr
            << "Note: one or more matrix uniforms are inactive. "
            << "This is expected if the current shader experiment does not use them.\n";
    }

    if (timeLocation == -1)
    {
        std::cerr
            << "Note: the time uniform is inactive. "
            << "This is expected if the current shader experiment does not use it.\n";
    }

    // A fixed rotation exposes several faces while keeping the known-good image
    // stable and easy to compare between runs.
    glm::mat4 model(1.0f);
    model = glm::rotate(model, glm::radians(20.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Positions and normals transform differently. The inverse-transpose keeps
    // normals perpendicular to their surfaces, including under non-uniform scale.
    const glm::mat3 normalMatrix =
        glm::transpose(glm::inverse(glm::mat3(model)));

    // The view matrix converts world-space positions into view space. Moving the
    // world by the negative viewer position places the cube in front of the
    // viewer without introducing a camera class or camera controls.
    const glm::vec3 viewPosition(0.0f, 0.0f, 3.0f);
    const glm::mat4 view =
        glm::translate(glm::mat4(1.0f), -viewPosition);

    // This direction points from the surface toward the light. It is not axis-
    // aligned, so more than one visible face receives diffuse illumination.
    const glm::vec3 lightDirection =
        glm::normalize(glm::vec3(0.6f, 1.0f, 0.8f));
    const glm::vec3 lightColor(1.0f, 0.96f, 0.90f);
    // White leaves the generated texture's sampled RGB values untinted.
    const glm::vec3 baseColor(1.0f);
    const float ambientStrength = 0.12f;
    const float specularStrength = 0.28f;
    const float shininess = 32.0f;

    // These values define the perspective viewing volume. Keeping them named and
    // visible makes it easy to ask: what changes when the field of view narrows,
    // or when the near and far clipping planes move?
    const float fieldOfView = glm::radians(45.0f);
    const float nearPlane = 0.1f;
    const float farPlane = 100.0f;

    while (glfwWindowShouldClose(window) == GLFW_FALSE)
    {
        processInput(window, screenMode, iterationCount, discardOrder, vsyncEnabled);

        // Framebuffer dimensions can differ from window dimensions on high-DPI
        // displays. Reading the current framebuffer size keeps projected shapes
        // in the correct proportions after a resize. A minimized window may have
        // no drawable area, so wait for events instead of dividing by zero.
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

        if (framebufferWidth == 0 || framebufferHeight == 0)
        {
            // Keep the old storage until there is a drawable area again.
            // Waiting processes events without repeatedly drawing an empty window.
            glfwWaitEvents();
            continue;
        }

        // Window resizing does not resize our attachments automatically.
        // Allocate on first use, then only when the drawable dimensions change.
        if (framebufferWidth != sceneWidth || framebufferHeight != sceneHeight)
        {
            if (!resizeSceneFramebuffer(sceneFramebuffer, sceneColorTexture,
                                        sceneDepthStencil, framebufferWidth, framebufferHeight))
            {
                exitCode = 1;
                break; // Use the same resource cleanup as normal shutdown.
            }
            sceneWidth = framebufferWidth;
            sceneHeight = framebufferHeight;
        }

        const float aspectRatio =
            static_cast<float>(framebufferWidth) /
            static_cast<float>(framebufferHeight);
        const glm::mat4 projection =
            glm::perspective(fieldOfView, aspectRatio, nearPlane, farPlane);

        // PASS 1: render the scene into the off-screen framebuffer.
        // Binding selects where clears and fragment outputs go. The viewport
        // maps clip-space results into that target; binding an FBO does not set it.
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
        glViewport(0, 0, sceneWidth, sceneHeight);
        glEnable(GL_DEPTH_TEST);
        // Depth must be enabled again each frame because pass 2 disables it.
        // Clear last frame's colour and depth before resolving cube visibility.
        glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // glm::value_ptr exposes each GLM matrix as contiguous float data.
        // GL_FALSE means OpenGL should use the conventional GLM/OpenGL matrix
        // layout directly, without transposing it during the upload.
        glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLocation, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(
            projectionLocation, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix3fv(
            normalMatrixLocation, 1, GL_FALSE, glm::value_ptr(normalMatrix));
        glUniform3fv(lightDirectionLocation, 1, glm::value_ptr(lightDirection));
        glUniform3fv(lightColorLocation, 1, glm::value_ptr(lightColor));
        glUniform3fv(viewPositionLocation, 1, glm::value_ptr(viewPosition));
        glUniform3fv(baseColorLocation, 1, glm::value_ptr(baseColor));
        glUniform1f(ambientStrengthLocation, ambientStrength);
        glUniform1f(specularStrengthLocation, specularStrength);
        glUniform1f(shininessLocation, shininess);
        glUniform1i(surfaceTextureLocation, 0);
        glUniform1f(timeLocation, static_cast<float>(glfwGetTime()));

        // Restore the cube's surface texture: pass 2 used this same unit for
        // the scene image. The cube must not sample the target it is writing into.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // PASS 2: render the scene texture to the default framebuffer.
        // Framebuffer 0 is the window's default framebuffer. The scene texture
        // is now an input to this draw, while the window's back buffer is the output.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        // We are displaying an image across the whole viewport, not resolving
        // 3D surface visibility. This pass does not need depth testing or a depth clear.
        glDisable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT);
        GLuint activeScreenProgram = screenProgram;
        GLint activeTextureLocation = sceneTextureLocation;
        if (screenMode == ScreenMode::Workload)
        {
            activeScreenProgram = workloadProgram;
            activeTextureLocation = workloadTextureLocation;
        }
        else if (screenMode == ScreenMode::Discard)
        {
            activeScreenProgram = discardProgram;
            activeTextureLocation = discardTextureLocation;
        }
        glUseProgram(activeScreenProgram);
        // Uniform uploads affect the current program. A sampler stores a unit
        // index (0), not a texture object name (sceneColorTexture).
        glUniform1i(activeTextureLocation, 0); // Sampler stores a texture-unit index.
        if (screenMode == ScreenMode::Workload)
        {
            glUniform1i(iterationCountLocation, iterationCount);
        }
        else if (screenMode == ScreenMode::Discard)
        {
            glUniform1i(discardIterationLocation, iterationCount);
            glUniform1i(discardOrderLocation, discardOrder);
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
        glBindVertexArray(screenVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();

        const auto now = std::chrono::steady_clock::now();
        frameTimeSumMs +=
            std::chrono::duration<double, std::milli>(now - previousFrameTime).count();
        previousFrameTime = now;
        if (++measuredFrames == 120)
        {
            const double averageMs = frameTimeSumMs / measuredFrames;
            std::cout << "Observed frame time (whole frame, last 120): "
                      << averageMs << " ms, ~" << (1000.0 / averageMs) << " FPS\n";
            frameTimeSumMs = 0.0;
            measuredFrames = 0;
        }
    }

    // Delete GPU resources while the OpenGL context still exists. Deleting the
    // framebuffer does not delete its attachments: each object has its own lifetime.
    glDeleteProgram(screenProgram);
    glDeleteProgram(workloadProgram);
    glDeleteProgram(discardProgram);
    glDeleteBuffers(1, &screenVbo);
    glDeleteVertexArrays(1, &screenVao);
    glDeleteFramebuffers(1, &sceneFramebuffer);
    glDeleteTextures(1, &sceneColorTexture);
    glDeleteRenderbuffers(1, &sceneDepthStencil);
    glDeleteProgram(shaderProgram);
    glDeleteTextures(1, &texture);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    glfwDestroyWindow(window);
    glfwTerminate();

    return exitCode;
}
