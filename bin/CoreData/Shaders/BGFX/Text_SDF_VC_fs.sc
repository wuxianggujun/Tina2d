$input v_color0, v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);

void main()
{
    vec4 tex = texture2D(s_texColor, v_texcoord0);
    float dist = tex.a;
    float w = max(fwidth(dist), 1e-3);
    float edge = 0.5;
    float t = saturate((dist - (edge - w)) / (2.0 * w));
    float alpha = t * v_color0.a;
    vec3 rgb = v_color0.rgb;
    gl_FragColor = vec4(rgb, alpha);
}

