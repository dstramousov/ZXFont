R"GLSL(#version 330 core
in vec2 vUv;
out vec4 fragColor;
uniform sampler2D uImage;
uniform vec2 uImageSize;
uniform float uCurvature;
uniform float uGlow;
uniform float uScanlines;
uniform float uVignette;

vec3 phosphor(vec2 uv) {
    vec4 pixel = texture(uImage, uv);
    return pixel.rgb * pixel.a;
}

void main() {
    vec2 screen = vUv * 2.0 - 1.0;
    float r2 = dot(screen, screen);
    // Inverse barrel mapping: screen coordinates -> curved source coordinates.
    vec2 mapped = screen * (1.0 + uCurvature * r2);
    vec2 uv = mapped * 0.5 + 0.5;
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    // Source RGBA buffers are top-left origin; OpenGL texture sampling is bottom-left.
    vec2 sampleUv = vec2(uv.x, 1.0 - uv.y);
    vec3 sharp = phosphor(sampleUv);
    vec2 texel = 1.0 / uImageSize;
    // Skip the eight extra texture samples when glow is disabled.
    vec3 color = sharp;
    if (uGlow > 0.0) {
        vec3 halo = (phosphor(sampleUv + vec2(texel.x, 0.0)) +
                     phosphor(sampleUv - vec2(texel.x, 0.0)) +
                     phosphor(sampleUv + vec2(0.0, texel.y)) +
                     phosphor(sampleUv - vec2(0.0, texel.y)) +
                     0.5 * phosphor(sampleUv + texel) +
                     0.5 * phosphor(sampleUv - texel) +
                     0.5 * phosphor(sampleUv + vec2(texel.x, -texel.y)) +
                     0.5 * phosphor(sampleUv + vec2(-texel.x, texel.y))) / 6.0;
        // Preserve sharp source pixels; glow is strictly additive.
        color += uGlow * halo * 0.65;
    }
    float scan = 1.0 - uScanlines *
                 pow(abs(fract(uv.y * uImageSize.y) - 0.5) * 2.0, 2.0) * 0.7;
    float vignette = 1.0 - uVignette * smoothstep(0.2, 1.75, r2);
    fragColor = vec4(clamp(color * scan * vignette, 0.0, 1.0), 1.0);
}
)GLSL"
