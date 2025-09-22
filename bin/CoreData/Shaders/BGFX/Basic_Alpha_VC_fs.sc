$input v_color0, v_texcoord0

#include "bgfx_shader.sh"

// 字体/Alpha 纹理：A 通道与顶点 Alpha 相乘
SAMPLER2D(s_texColor, 0);

void main()
{
    float a = texture2D(s_texColor, v_texcoord0).a;
    vec3 rgb = v_color0.rgb;
    gl_FragColor = vec4(rgb, a * v_color0.a);
}

