// FunScreen
//
// Creates a click-through, always-on-top, borderless window covering one
// monitor. Continuously captures that monitor's contents (DXGI Desktop
// Duplication), uploads it as a texture, and draws it through a live-
// reloadable GLSL fragment shader (shaders/default.frag).
//
// Workflow: edit shaders/default.frag in your normal text editor, save it.
// The app polls the file's last-write time and hot-recompiles automatically.
// Press Ctrl+Alt+F5 to force a manual recompile too.
//
// Compile errors print to the console window this app opens on startup.

#include <windows.h>
#include <glad/glad.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>

#include "Capture.h"
#include "ShaderUtils.h"

#pragma comment(lib, "opengl32.lib")

// ---------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------
static const wchar_t* kWindowClass = L"ShaderOverlayWindowClass";
//static const char* kShaderPath = "shaders/default.frag";
static const int kHotkeyId = 1;

// ---------------------------------------------------------------------
// Embedded vertex shader (fullscreen triangle, no vertex buffer needed).
// ---------------------------------------------------------------------
static const char* kVertexShaderSrc = R"(
    #version 330 core
    out vec2 vUV;
    void main() {
        vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
        vUV = pos;
        gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
    }
)";

static const char* kFallbackFragmentShaderSrc = R"(
    #version 330 core
    in vec2 vUV;
    out vec4 fragColor;
    uniform sampler2D u_screen;
    void main() {
        vec2 uv = vUV; uv.y = 1.0 - uv.y;
        fragColor = texture(u_screen, uv);
    }
)";

// ---------------------------------------------------------------------
// Gets the path of this exe
// ---------------------------------------------------------------------
std::string GetExeDir()
{
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string p(path);
    size_t pos = p.find_last_of("\\/");

    return p.substr(0, pos);
}

// ---------------------------------------------------------------------
// Shared state between render loop and file-watcher thread
// ---------------------------------------------------------------------
static std::atomic<bool> reloadRequested{true}; // trigger initial load
static std::atomic<bool> running{true};

static std::string ReadFile(const std::string& path, bool& ok)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { ok = false; return ""; }
    std::ostringstream ss;
    ss << f.rdbuf();
    ok = true;

    return ss.str();
}

// Polls the shader file's last-write time; sets g_reloadRequested on change.
void ShaderWatcherThread(const std::string& shaderPath)
{
    FILETIME lastWrite = {};
    while (running)
    {
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (GetFileAttributesExA(shaderPath.c_str(), GetFileExInfoStandard, &data))
        {
            if (CompareFileTime(&data.ftLastWriteTime, &lastWrite) != 0)
            {
                lastWrite = data.ftLastWriteTime;

                // small debounce so we don't read a half-written file
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
                reloadRequested = true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

// ---------------------------------------------------------------------
// Win32 boilerplate
// ---------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_HOTKEY:
        {
            if (wParam == kHotkeyId)
            {
                reloadRequested = true;
            }

            return 0;
        }
        case WM_DESTROY:
        {
            running = false;
            PostQuitMessage(0);

            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/*

struct MonitorRect { RECT rect; };

// Finds the Nth monitor's rect (in virtual-desktop coordinates).
bool GetMonitorRect(int index, RECT& outRect)
{
    struct EnumCtx { int target; int count; RECT result; bool found; } ctx{index, 0, {}, false};

    EnumDisplayMonitors(nullptr, nullptr,
        [](HMONITOR hMon, HDC, LPRECT, LPARAM lp) -> BOOL
        {
            auto* ctx = reinterpret_cast<EnumCtx*>(lp);
            if (ctx->count == ctx->target)
            {
                MONITORINFO mi = { sizeof(mi) };
                GetMonitorInfo(hMon, &mi);
                ctx->result = mi.rcMonitor;
                ctx->found = true;
            }
            ctx->count++;
            return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));

    if (ctx.found) outRect = ctx.result;
    return ctx.found;
}

*/

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int)
{
    // ------------------------------------------------------------------------
    // Console
    // ------------------------------------------------------------------------

    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);

    // ------------------------------------------------------------------------
    // Getting Shader Path + Output Messages
    // ------------------------------------------------------------------------

    std::string shaderPath = GetExeDir() + "\\shaders\\default.frag";

    printf("FunScreen starting. Edit %s and save to hot-reload.\n", shaderPath.c_str());
    printf("Press Ctrl+Alt+F5 to force-recompile. Close this console to quit.\n\n");

    int monitorIndex = 0;
    if (pCmdLine && pCmdLine[0])
    {
        monitorIndex = _wtoi(pCmdLine);
    }

    /*
    RECT monRect;
    if (!GetMonitorRect(monitorIndex, monRect))
    {
        printf("Monitor index %d not found, falling back to monitor 0.\n", monitorIndex);
        monitorIndex = 0;
        GetMonitorRect(0, monRect);
    }
    int winW = monRect.right - monRect.left;
    int winH = monRect.bottom - monRect.top;
    */

    DesktopDuplicator duplicator;
    if (!duplicator.Init(monitorIndex))
    {
        printf("Failed to initialize screen capture for monitor %d.\n", monitorIndex);
        return 1;
    }
    printf("Capturing monitor %d (%ux%u)\n", monitorIndex, duplicator.Width(), duplicator.Height());

    RECT monRect = duplicator.GetDesktopRect();
    int winW = monRect.right - monRect.left;
    int winH = monRect.bottom - monRect.top;

    // ------------------------------------------------------------------------
    // Register and Create the Overlay Window
    // ------------------------------------------------------------------------

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kWindowClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // The see-thru window
    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED |      // https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features#layered-windows
        WS_EX_TOPMOST |      // always on top.
        WS_EX_TRANSPARENT |  // clicks pass through to windows below.
        WS_EX_TOOLWINDOW |   // no taskbar entry.
        WS_EX_NOACTIVATE,    // never steals keyboard focus.
        kWindowClass, 
        L"FunScreen",
        WS_POPUP,
        monRect.left,
        monRect.top,
        winW,
        winH,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    // Because we used: WS_EX_LAYERED
    // bAlpha = 255 for opaque
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

    // Ctrl + Alt + F5
    RegisterHotKey(hwnd, kHotkeyId, MOD_CONTROL | MOD_ALT, VK_F5);

    // The window is displayed on the monitor. Everywhere else, the window does not appear!
    SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);

    // ------------------------------------------------------------------------
    // OpenGL Context Setup
    // ------------------------------------------------------------------------

    // Handle to device context (DC)
    HDC hdc = GetDC(hwnd);

    // Pixel format descriptor
    PIXELFORMATDESCRIPTOR pixelFormatDesc = { sizeof(pixelFormatDesc), 1 };
    pixelFormatDesc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixelFormatDesc.iPixelType = PFD_TYPE_RGBA;
    pixelFormatDesc.cColorBits = 32;

    // 
    int pixelFormat = ChoosePixelFormat(hdc, &pixelFormatDesc);
    SetPixelFormat(hdc, pixelFormat, &pixelFormatDesc);

    // Create new OpenGl rendering context, suitable for drawing on the device referenced by hdc
    HGLRC hglrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hglrc);

    // Init Glad
    if (!gladLoadGL())
    {
        printf("gladLoadGL failed - GPU/driver may not support the required OpenGL version.\n");
        return 1;
    }
    printf("OpenGL: %s\n", glGetString(GL_VERSION));

    // ------------------------------------------------------------------------
    // ------------------------------------------------------------------------

    // Display the window
    ShowWindow(hwnd, SW_SHOW);

    // ------------------------------------------------------------------------
    // Capture Setup
    // ------------------------------------------------------------------------

    // Setup the screen texture
    GLuint screenTex;
    glGenTextures(1, &screenTex);
    glBindTexture(GL_TEXTURE_2D, screenTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, duplicator.Width(), duplicator.Height(), 0, GL_BGRA, GL_UNSIGNED_BYTE,
                 nullptr);

    // Core profile requires some VAO bound even for bufferless drawing.
    GLuint dummyVao;
    glGenVertexArrays(1, &dummyVao);
    glBindVertexArray(dummyVao);

    GLuint activeProgram = 0;
    {
        std::string log;
        activeProgram = BuildProgramFromSource(kVertexShaderSrc, kFallbackFragmentShaderSrc, log);
    }

    std::thread watcher(ShaderWatcherThread, shaderPath);

    std::vector<uint8_t> frameBuf;
    auto startTime = std::chrono::steady_clock::now();

    // ------------------------------------------------------------------------
    // Main Loop
    // ------------------------------------------------------------------------

    MSG msg;
    while (running)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // If hotkey is pressed or file changes
        if (reloadRequested.exchange(false))
        {
            // If the file is valid and readable
            bool ok;
            std::string src = ReadFile(shaderPath, ok);
            if (ok)
            {
                std::string log;

                // Compile shader and build program
                GLuint newProgram = BuildProgramFromSource(kVertexShaderSrc, src, log);
                if (newProgram)  // Shader is valid
                {
                    if (activeProgram) glDeleteProgram(activeProgram);
                    activeProgram = newProgram;
                    printf("[%s] shader reloaded OK\n", shaderPath.c_str());
                }
                else  // Shader is invalid
                {
                    printf("[%s] compile FAILED, keeping previous shader:\n%s\n", shaderPath.c_str(), log.c_str());
                }
            }
            else  // Can't even read shader
            {
                printf("Could not read %s\n", shaderPath.c_str());
            }
        }

        // Pass the frame
        if (duplicator.GetFrame(frameBuf))
        {
            // Allocate the texture
            glBindTexture(GL_TEXTURE_2D, screenTex);

            // Update pixel data (Sampler2D... texture unit 0)
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, duplicator.Width(), duplicator.Height(), GL_BGRA, GL_UNSIGNED_BYTE,
                            frameBuf.data());
        }

        float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();

        glViewport(0, 0, winW, winH);
        glUseProgram(activeProgram);

        // Get uniform locations and pass in the data to shader if valid
        GLint texLoc  = glGetUniformLocation(activeProgram, "u_screen");
        GLint resLoc  = glGetUniformLocation(activeProgram, "u_resolution");
        GLint timeLoc = glGetUniformLocation(activeProgram, "u_time");
        if (texLoc  >= 0) glUniform1i(texLoc, 0);
        if (resLoc  >= 0) glUniform2f(resLoc, (float)winW, (float)winH);
        if (timeLoc >= 0) glUniform1f(timeLoc, elapsed);

        // Rendering stuff
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, screenTex);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        SwapBuffers(hdc);
    }

    watcher.join();
    duplicator.Shutdown();
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);

    return 0;
}
