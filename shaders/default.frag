#version 330 core

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D u_screen;     // your captured desktop
uniform vec2      u_resolution; // window size in pixels
uniform float     u_time;       // seconds since app start

void main() {
    vec2 uv = vUV;
    uv.y = 1.0 - uv.y; // capture is top-down, GL UV is bottom-up

    vec3 color = texture(u_screen, uv).rgb;

    // Wavy distortion:
    // uv.x += sin(uv.y * 40.0 + u_time * 20.0) * 0.004;
    // color = texture(u_screen, uv).rgb;

    // Invert colors:
    // color = 1.0 - color;

    // Grayscale:
    // float gray = dot(color, vec3(0.299, 0.587, 0.114));
    // color = vec3(gray);

    // Chromatic aberration:
    // float shift = 0.01;
    // color.r = texture(u_screen, uv + vec2(shift, 0.0)).r;
    // color.b = texture(u_screen, uv - vec2(shift, 0.0)).b;

    fragColor = vec4(color, 1.0);
}
