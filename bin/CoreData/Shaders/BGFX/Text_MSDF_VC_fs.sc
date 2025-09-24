$input v_color0, v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);

// x=edge(默认0.5), y=softnessScale(默认1.0), z,w 保留
uniform vec4 u_msdfParams;

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
    vec3 m = texture2D(s_texColor, v_texcoord0).rgb;
    float dist = median(m.r, m.g, m.b);

    float w = max(fwidth(dist), 1e-4) * max(u_msdfParams.y, 1e-4);
    float edge = (u_msdfParams.x > 0.0) ? u_msdfParams.x : 0.5;
    float t = saturate((dist - (edge - w)) / (2.0 * w));

    float alpha = t * v_color0.a;
    vec3 rgb = v_color0.rgb;
    gl_FragColor = vec4(rgb, alpha);
}

