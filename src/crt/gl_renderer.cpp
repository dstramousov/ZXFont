#include <zxcrt/zxcrt.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace zxcrt {
namespace {

#ifdef _WIN32
#define ZXCRT_GL_CALL __stdcall
#else
#define ZXCRT_GL_CALL
#endif

using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLboolean = unsigned char;
using GLfloat = float;
using GLchar = char;

constexpr GLenum kVertexShader = 0x8B31;
constexpr GLenum kFragmentShader = 0x8B30;
constexpr GLenum kCompileStatus = 0x8B81;
constexpr GLenum kLinkStatus = 0x8B82;
constexpr GLenum kInfoLogLength = 0x8B84;
constexpr GLenum kTexture2D = 0x0DE1;
constexpr GLenum kTextureBinding2D = 0x8069;
constexpr GLenum kTexture0 = 0x84C0;
constexpr GLenum kActiveTexture = 0x84E0;
constexpr GLenum kTextureMinFilter = 0x2801;
constexpr GLenum kTextureMagFilter = 0x2800;
constexpr GLenum kTextureWrapS = 0x2802;
constexpr GLenum kTextureWrapT = 0x2803;
constexpr GLint kNearest = 0x2600;
constexpr GLint kClampToEdge = 0x812F;
constexpr GLenum kRgba = 0x1908;
constexpr GLenum kRgba8 = 0x8058;
constexpr GLenum kUnsignedByte = 0x1401;
constexpr GLenum kUnpackAlignment = 0x0CF5;
constexpr GLenum kCurrentProgram = 0x8B8D;
constexpr GLenum kVertexArrayBinding = 0x85B5;
constexpr GLenum kViewport = 0x0BA2;
constexpr GLenum kColorClearValue = 0x0C22;
constexpr GLenum kColorBufferBit = 0x00004000;
constexpr GLenum kTriangles = 0x0004;
constexpr GLenum kDepthTest = 0x0B71;
constexpr GLenum kBlend = 0x0BE2;
constexpr GLenum kScissorTest = 0x0C11;
constexpr GLenum kCullFace = 0x0B44;

constexpr const char* kVertexSource =
#include "shaders/fullscreen.vert.inc"
    ;
constexpr const char* kFragmentSource =
#include "shaders/crt.frag.inc"
    ;

// Signatures from the OpenGL 3.3 core specification; the loader comes from
// the host application's current GL context, not from an SDL dependency.
struct GL {
#define ZXCRT_GL_PROC(name, result, ...) \
    using name##Proc = result (ZXCRT_GL_CALL*)(__VA_ARGS__); \
    name##Proc name = nullptr
    ZXCRT_GL_PROC(CreateShader, GLuint, GLenum);
    ZXCRT_GL_PROC(ShaderSource, void, GLuint, GLsizei, const GLchar* const*, const GLint*);
    ZXCRT_GL_PROC(CompileShader, void, GLuint);
    ZXCRT_GL_PROC(GetShaderiv, void, GLuint, GLenum, GLint*);
    ZXCRT_GL_PROC(GetShaderInfoLog, void, GLuint, GLsizei, GLsizei*, GLchar*);
    ZXCRT_GL_PROC(DeleteShader, void, GLuint);
    ZXCRT_GL_PROC(CreateProgram, GLuint);
    ZXCRT_GL_PROC(AttachShader, void, GLuint, GLuint);
    ZXCRT_GL_PROC(LinkProgram, void, GLuint);
    ZXCRT_GL_PROC(GetProgramiv, void, GLuint, GLenum, GLint*);
    ZXCRT_GL_PROC(GetProgramInfoLog, void, GLuint, GLsizei, GLsizei*, GLchar*);
    ZXCRT_GL_PROC(DeleteProgram, void, GLuint);
    ZXCRT_GL_PROC(UseProgram, void, GLuint);
    ZXCRT_GL_PROC(GetUniformLocation, GLint, GLuint, const GLchar*);
    ZXCRT_GL_PROC(Uniform1i, void, GLint, GLint);
    ZXCRT_GL_PROC(Uniform1f, void, GLint, GLfloat);
    ZXCRT_GL_PROC(Uniform2f, void, GLint, GLfloat, GLfloat);
    ZXCRT_GL_PROC(GenVertexArrays, void, GLsizei, GLuint*);
    ZXCRT_GL_PROC(BindVertexArray, void, GLuint);
    ZXCRT_GL_PROC(DeleteVertexArrays, void, GLsizei, const GLuint*);
    ZXCRT_GL_PROC(GenTextures, void, GLsizei, GLuint*);
    ZXCRT_GL_PROC(BindTexture, void, GLenum, GLuint);
    ZXCRT_GL_PROC(DeleteTextures, void, GLsizei, const GLuint*);
    ZXCRT_GL_PROC(ActiveTexture, void, GLenum);
    ZXCRT_GL_PROC(TexParameteri, void, GLenum, GLenum, GLint);
    ZXCRT_GL_PROC(TexImage2D, void, GLenum, GLint, GLint, GLsizei, GLsizei,
                  GLint, GLenum, GLenum, const void*);
    ZXCRT_GL_PROC(TexSubImage2D, void, GLenum, GLint, GLint, GLint, GLsizei,
                     GLsizei, GLenum, GLenum, const void*);
    ZXCRT_GL_PROC(PixelStorei, void, GLenum, GLint);
    ZXCRT_GL_PROC(Viewport, void, GLint, GLint, GLsizei, GLsizei);
    ZXCRT_GL_PROC(ClearColor, void, GLfloat, GLfloat, GLfloat, GLfloat);
    ZXCRT_GL_PROC(Clear, void, GLenum);
    ZXCRT_GL_PROC(DrawArrays, void, GLenum, GLint, GLsizei);
    ZXCRT_GL_PROC(GetIntegerv, void, GLenum, GLint*);
    ZXCRT_GL_PROC(GetFloatv, void, GLenum, GLfloat*);
    ZXCRT_GL_PROC(IsEnabled, GLboolean, GLenum);
    ZXCRT_GL_PROC(Enable, void, GLenum);
    ZXCRT_GL_PROC(Disable, void, GLenum);
#undef ZXCRT_GL_PROC

    explicit GL(GLProcLoader loader) {
        if (loader == nullptr) {
            throw std::invalid_argument("CRT GL procedure loader must not be null");
        }
#define ZXCRT_GL_LOAD(name) \
        name = reinterpret_cast<name##Proc>(loader("gl" #name)); \
        if (name == nullptr) { \
            throw std::runtime_error("OpenGL 3.3 entry point unavailable: gl" #name); \
        }
        ZXCRT_GL_LOAD(CreateShader)
        ZXCRT_GL_LOAD(ShaderSource)
        ZXCRT_GL_LOAD(CompileShader)
        ZXCRT_GL_LOAD(GetShaderiv)
        ZXCRT_GL_LOAD(GetShaderInfoLog)
        ZXCRT_GL_LOAD(DeleteShader)
        ZXCRT_GL_LOAD(CreateProgram)
        ZXCRT_GL_LOAD(AttachShader)
        ZXCRT_GL_LOAD(LinkProgram)
        ZXCRT_GL_LOAD(GetProgramiv)
        ZXCRT_GL_LOAD(GetProgramInfoLog)
        ZXCRT_GL_LOAD(DeleteProgram)
        ZXCRT_GL_LOAD(UseProgram)
        ZXCRT_GL_LOAD(GetUniformLocation)
        ZXCRT_GL_LOAD(Uniform1i)
        ZXCRT_GL_LOAD(Uniform1f)
        ZXCRT_GL_LOAD(Uniform2f)
        ZXCRT_GL_LOAD(GenVertexArrays)
        ZXCRT_GL_LOAD(BindVertexArray)
        ZXCRT_GL_LOAD(DeleteVertexArrays)
        ZXCRT_GL_LOAD(GenTextures)
        ZXCRT_GL_LOAD(BindTexture)
        ZXCRT_GL_LOAD(DeleteTextures)
        ZXCRT_GL_LOAD(ActiveTexture)
        ZXCRT_GL_LOAD(TexParameteri)
        ZXCRT_GL_LOAD(TexImage2D)
        ZXCRT_GL_LOAD(TexSubImage2D)
        ZXCRT_GL_LOAD(PixelStorei)
        ZXCRT_GL_LOAD(Viewport)
        ZXCRT_GL_LOAD(ClearColor)
        ZXCRT_GL_LOAD(Clear)
        ZXCRT_GL_LOAD(DrawArrays)
        ZXCRT_GL_LOAD(GetIntegerv)
        ZXCRT_GL_LOAD(GetFloatv)
        ZXCRT_GL_LOAD(IsEnabled)
        ZXCRT_GL_LOAD(Enable)
        ZXCRT_GL_LOAD(Disable)
#undef ZXCRT_GL_LOAD
    }
};

GLuint compileShader(const GL& gl, GLenum stage, const char* source) {
    const GLuint shader = gl.CreateShader(stage);
    if (shader == 0) {
        throw std::runtime_error("OpenGL could not allocate a shader");
    }
    gl.ShaderSource(shader, 1, &source, nullptr);
    gl.CompileShader(shader);
    GLint compiled = 0;
    gl.GetShaderiv(shader, kCompileStatus, &compiled);
    if (compiled == 0) {
        GLint length = 0;
        gl.GetShaderiv(shader, kInfoLogLength, &length);
        std::string log(static_cast<std::size_t>(length > 1 ? length : 1), '\0');
        gl.GetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        gl.DeleteShader(shader);
        throw std::runtime_error("CRT shader compilation failed: " + log);
    }
    return shader;
}

GLuint linkProgram(const GL& gl, GLuint vertex, GLuint fragment) {
    const GLuint program = gl.CreateProgram();
    if (program == 0) {
        throw std::runtime_error("OpenGL could not allocate a program");
    }
    gl.AttachShader(program, vertex);
    gl.AttachShader(program, fragment);
    gl.LinkProgram(program);
    GLint linked = 0;
    gl.GetProgramiv(program, kLinkStatus, &linked);
    if (linked == 0) {
        GLint length = 0;
        gl.GetProgramiv(program, kInfoLogLength, &length);
        std::string log(static_cast<std::size_t>(length > 1 ? length : 1), '\0');
        gl.GetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        gl.DeleteProgram(program);
        throw std::runtime_error("CRT shader linking failed: " + log);
    }
    return program;
}

struct GLState {
    GLint program = 0;
    GLint vertexArray = 0;
    GLint activeTexture = 0;
    GLint texture = 0;
    GLint unpack = 4;
    std::array<GLint, 4> viewport{};
    std::array<GLfloat, 4> clearColor{};
    GLboolean depth = 0;
    GLboolean blend = 0;
    GLboolean scissor = 0;
    GLboolean cull = 0;

    explicit GLState(const GL& gl) {
        gl.GetIntegerv(kCurrentProgram, &program);
        gl.GetIntegerv(kVertexArrayBinding, &vertexArray);
        gl.GetIntegerv(kActiveTexture, &activeTexture);
        gl.ActiveTexture(kTexture0);
        gl.GetIntegerv(kTextureBinding2D, &texture);
        gl.GetIntegerv(kUnpackAlignment, &unpack);
        gl.GetIntegerv(kViewport, viewport.data());
        gl.GetFloatv(kColorClearValue, clearColor.data());
        depth = gl.IsEnabled(kDepthTest);
        blend = gl.IsEnabled(kBlend);
        scissor = gl.IsEnabled(kScissorTest);
        cull = gl.IsEnabled(kCullFace);
    }

    void restore(const GL& gl) const noexcept {
        gl.UseProgram(static_cast<GLuint>(program));
        gl.BindVertexArray(static_cast<GLuint>(vertexArray));
        gl.BindTexture(kTexture2D, static_cast<GLuint>(texture));
        gl.ActiveTexture(static_cast<GLenum>(activeTexture));
        gl.PixelStorei(kUnpackAlignment, unpack);
        gl.Viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        gl.ClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        const auto set = [&](GLenum cap, GLboolean enabled) {
            if (enabled != 0) gl.Enable(cap);
            else gl.Disable(cap);
        };
        set(kDepthTest, depth);
        set(kBlend, blend);
        set(kScissorTest, scissor);
        set(kCullFace, cull);
    }
};

}  // namespace

struct Renderer::Impl {
    explicit Impl(GLProcLoader loader) : gl(loader) {}

    ~Impl() {
        // Caller must keep the same GL context current until destruction.
        if (texture != 0) gl.DeleteTextures(1, &texture);
        if (vao != 0) gl.DeleteVertexArrays(1, &vao);
        if (program != 0) gl.DeleteProgram(program);
    }

    void initialize() {
        const GLuint vertex = compileShader(gl, kVertexShader, kVertexSource);
        GLuint fragment = 0;
        try {
            fragment = compileShader(gl, kFragmentShader, kFragmentSource);
            program = linkProgram(gl, vertex, fragment);
        } catch (...) {
            gl.DeleteShader(vertex);
            if (fragment != 0) gl.DeleteShader(fragment);
            throw;
        }
        gl.DeleteShader(vertex);
        gl.DeleteShader(fragment);

        gl.GenVertexArrays(1, &vao);
        gl.GenTextures(1, &texture);
        if (vao == 0 || texture == 0) {
            throw std::runtime_error("CRT could not allocate GPU objects");
        }

        const GLState state(gl);
        gl.BindTexture(kTexture2D, texture);
        gl.TexParameteri(kTexture2D, kTextureMinFilter, kNearest);
        gl.TexParameteri(kTexture2D, kTextureMagFilter, kNearest);
        gl.TexParameteri(kTexture2D, kTextureWrapS, kClampToEdge);
        gl.TexParameteri(kTexture2D, kTextureWrapT, kClampToEdge);
        state.restore(gl);

        imageLocation = gl.GetUniformLocation(program, "uImage");
        sizeLocation = gl.GetUniformLocation(program, "uImageSize");
        curvatureLocation = gl.GetUniformLocation(program, "uCurvature");
        glowLocation = gl.GetUniformLocation(program, "uGlow");
        scanlinesLocation = gl.GetUniformLocation(program, "uScanlines");
        vignetteLocation = gl.GetUniformLocation(program, "uVignette");
        if (imageLocation < 0 || sizeLocation < 0 || curvatureLocation < 0 ||
            glowLocation < 0 || scanlinesLocation < 0 || vignetteLocation < 0) {
            throw std::runtime_error("CRT required shader uniforms are missing");
        }
    }

    GL gl;
    GLuint program = 0;
    GLuint vao = 0;
    GLuint texture = 0;
    int sourceWidth = 0;
    int sourceHeight = 0;
    GLint imageLocation = -1;
    GLint sizeLocation = -1;
    GLint curvatureLocation = -1;
    GLint glowLocation = -1;
    GLint scanlinesLocation = -1;
    GLint vignetteLocation = -1;
};

Renderer::Renderer(GLProcLoader loader) : impl_(std::make_unique<Impl>(loader)) {
    impl_->initialize();
}

Renderer::~Renderer() = default;

void Renderer::setSettings(const Settings& settings) {
    validateSettings(settings);
    settings_ = settings;
}

const Settings& Renderer::settings() const noexcept { return settings_; }

void Renderer::render(std::span<const std::uint8_t> pixels, int sourceWidth,
                      int sourceHeight, int outputWidth, int outputHeight) {
    const Viewport rect = fitViewport(sourceWidth, sourceHeight,
                                      outputWidth, outputHeight);
    const auto area = static_cast<std::uint64_t>(sourceWidth) *
                      static_cast<std::uint64_t>(sourceHeight);
    if (area > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() / 4) ||
        pixels.size() != static_cast<std::size_t>(area) * 4) {
        throw std::invalid_argument("CRT requires exactly width * height * 4 RGBA8 bytes");
    }
    auto& inner = *impl_;
    const GLState old(inner.gl);
    inner.gl.Disable(kDepthTest);
    inner.gl.Disable(kBlend);
    inner.gl.Disable(kScissorTest);
    inner.gl.Disable(kCullFace);
    inner.gl.PixelStorei(kUnpackAlignment, 1);
    inner.gl.ActiveTexture(kTexture0);
    inner.gl.BindTexture(kTexture2D, inner.texture);
    if (sourceWidth != inner.sourceWidth || sourceHeight != inner.sourceHeight) {
        inner.gl.TexImage2D(kTexture2D, 0, static_cast<GLint>(kRgba8),
                            sourceWidth, sourceHeight, 0, kRgba, kUnsignedByte,
                            pixels.data());
        inner.sourceWidth = sourceWidth;
        inner.sourceHeight = sourceHeight;
    } else {
        inner.gl.TexSubImage2D(kTexture2D, 0, 0, 0, sourceWidth, sourceHeight,
                               kRgba, kUnsignedByte, pixels.data());
    }
    inner.gl.Viewport(0, 0, outputWidth, outputHeight);
    inner.gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    inner.gl.Clear(kColorBufferBit);
    inner.gl.Viewport(rect.x, rect.y, rect.width, rect.height);
    inner.gl.UseProgram(inner.program);
    inner.gl.Uniform1i(inner.imageLocation, 0);
    inner.gl.Uniform2f(inner.sizeLocation, static_cast<float>(sourceWidth),
                       static_cast<float>(sourceHeight));
    inner.gl.Uniform1f(inner.curvatureLocation, settings_.enabled ? settings_.curvature : 0.0f);
    inner.gl.Uniform1f(inner.glowLocation, settings_.enabled ? settings_.glow : 0.0f);
    inner.gl.Uniform1f(inner.scanlinesLocation, settings_.enabled ? settings_.scanlines : 0.0f);
    inner.gl.Uniform1f(inner.vignetteLocation, settings_.enabled ? settings_.vignette : 0.0f);
    inner.gl.BindVertexArray(inner.vao);
    inner.gl.DrawArrays(kTriangles, 0, 3);
    old.restore(inner.gl);
}

}  // namespace zxcrt
