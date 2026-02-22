#include "overlay.h"
#include "theme.h"
#include "ANativeWindowCreator.h"
#include "imgui.h"
#include "imgui_impl_android.h"
#include "imgui_impl_opengl3.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <unistd.h>
#include <cstdio>

static EGLDisplay g_display = EGL_NO_DISPLAY;
static EGLSurface g_surface = EGL_NO_SURFACE;
static EGLContext g_context = EGL_NO_CONTEXT;
static ANativeWindow* g_window = nullptr;
static int g_window_width = 0;
static int g_window_height = 0;
static bool g_imgui_android_inited = false;
static bool g_imgui_gl3_inited = false;

static void overlay_cleanup_egl() {
    if (g_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_surface != EGL_NO_SURFACE) {
            eglDestroySurface(g_display, g_surface);
            g_surface = EGL_NO_SURFACE;
        }
        if (g_context != EGL_NO_CONTEXT) {
            eglDestroyContext(g_display, g_context);
            g_context = EGL_NO_CONTEXT;
        }
        eglTerminate(g_display);
        g_display = EGL_NO_DISPLAY;
    }
}

bool overlay_init(int width, int height) {
    auto fail = [](const char* msg) -> bool {
        printf("[Overlay] ✗ %s\n", msg);
        ImGuiContext* ctx = ImGui::GetCurrentContext();
        if (ctx) {
            if (g_imgui_gl3_inited) {
                ImGui_ImplOpenGL3_Shutdown();
                g_imgui_gl3_inited = false;
            }
            if (g_imgui_android_inited) {
                ImGui_ImplAndroid_Shutdown();
                g_imgui_android_inited = false;
            }
            ImGui::DestroyContext();
        }
        overlay_cleanup_egl();
        if (g_window) {
            android::ANativeWindowCreator::Destroy(g_window);
            g_window = nullptr;
        }
        return false;
    };

    // 1. 创建 ANativeWindow (透明, 最顶层, 全屏)
    g_window = android::ANativeWindowCreator::Create("hphphp-overlay", width, height);
    if (!g_window) {
        printf("[Overlay] ✗ 无法创建ANativeWindow\n");
        return false;
    }
    g_window_width = width;
    g_window_height = height;

    // 2. EGL 初始化
    g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_display == EGL_NO_DISPLAY) {
        return fail("eglGetDisplay 失败");
    }
    if (eglInitialize(g_display, nullptr, nullptr) != EGL_TRUE) {
        return fail("eglInitialize 失败");
    }

    // 关键: EGL_ALPHA_SIZE=8 让覆盖层支持透明
    EGLint configAttribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE
    };
    EGLConfig config;
    EGLint numConfigs;
    if (eglChooseConfig(g_display, configAttribs, &config, 1, &numConfigs) != EGL_TRUE || numConfigs <= 0) {
        return fail("eglChooseConfig 失败");
    }

    g_surface = eglCreateWindowSurface(g_display, config, g_window, nullptr);
    if (g_surface == EGL_NO_SURFACE) {
        return fail("eglCreateWindowSurface 失败");
    }

    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    g_context = eglCreateContext(g_display, config, EGL_NO_CONTEXT, contextAttribs);
    if (g_context == EGL_NO_CONTEXT) {
        return fail("eglCreateContext 失败");
    }

    if (eglMakeCurrent(g_display, g_surface, g_surface, g_context) != EGL_TRUE) {
        return fail("eglMakeCurrent 失败");
    }

    // 3. ImGui 初始化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // 禁用 imgui.ini 保存
    io.IniFilename = nullptr;

    // 4. 加载中文字体
    ImFontConfig fontCfg;
    fontCfg.OversampleH = 1;
    fontCfg.OversampleV = 1;
    fontCfg.PixelSnapH = true;

    const char* fontPaths[] = {
        "/system/fonts/NotoSansCJK-Regular.ttc",
        "/system/fonts/DroidSansFallback.ttf",
        "/system/fonts/NotoSansSC-Regular.otf",
        "/system/fonts/Roboto-Regular.ttf"
    };
    bool fontLoaded = false;
    for (auto path : fontPaths) {
        if (access(path, R_OK) == 0) {
            io.Fonts->AddFontFromFileTTF(path, 32.0f, &fontCfg,
                io.Fonts->GetGlyphRangesChineseFull());
            fontLoaded = true;
            printf("[Overlay] ✓ 字体加载: %s\n", path);
            break;
        }
    }
    if (!fontLoaded) {
        io.Fonts->AddFontDefault();
        printf("[Overlay] ⚠ 使用默认字体\n");
    }

    // 5. 应用自定义主题 (淡白·柔粉·暖黄)
    ApplyHPHPTheme();

    // 6. 初始化后端
    if (!ImGui_ImplAndroid_Init(g_window)) {
        return fail("ImGui_ImplAndroid_Init 失败");
    }
    g_imgui_android_inited = true;
    if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
        return fail("ImGui_ImplOpenGL3_Init 失败");
    }
    g_imgui_gl3_inited = true;

    // 7. 开启混合 (透明渲染必需)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    printf("[Overlay] ✓ 初始化完成 (%dx%d)\n", width, height);
    return true;
}

void overlay_begin() {
    // 完全透明背景 — 关键!
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(g_window_width, g_window_height);
    ImGui::NewFrame();
}

void overlay_end() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (g_display != EGL_NO_DISPLAY && g_surface != EGL_NO_SURFACE) {
        eglSwapBuffers(g_display, g_surface);
    }
}

void overlay_destroy() {
    if (g_imgui_gl3_inited) {
        ImGui_ImplOpenGL3_Shutdown();
        g_imgui_gl3_inited = false;
    }
    if (g_imgui_android_inited) {
        ImGui_ImplAndroid_Shutdown();
        g_imgui_android_inited = false;
    }
    if (ImGui::GetCurrentContext()) {
        ImGui::DestroyContext();
    }

    overlay_cleanup_egl();
    if (g_window) {
        android::ANativeWindowCreator::Destroy(g_window);
        g_window = nullptr;
    }
    g_window_width = 0;
    g_window_height = 0;
}
