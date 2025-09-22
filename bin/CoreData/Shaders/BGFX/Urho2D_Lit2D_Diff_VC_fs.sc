$input v_color0, v_texcoord0, v_worldPos

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);

// x = count, y = ambient, z,w unused
uniform vec4 u_2dLightCountAmbient;
// xy = position (world), z = radius, w = type (1=POINT, 0=DIRECTIONAL)
uniform vec4 u_2dLightsPosRange[8];
// rgb = color, a = intensity
uniform vec4 u_2dLightsColorInt[8];

void main()
{
    vec4 diffInput = texture2D(s_texColor, v_texcoord0);
    vec4 base = diffInput * v_color0;

    float ambient = u_2dLightCountAmbient.y;
    vec3 add = vec3(ambient, ambient, ambient);
    int count = int(u_2dLightCountAmbient.x + 0.5);
    vec2 p = v_worldPos;

    for (int i = 0; i < 8; ++i)
    {
        if (i >= count)
            break;
        vec4 pr = u_2dLightsPosRange[i];
        vec4 ci = u_2dLightsColorInt[i];
        float att = 0.0;
        if (pr.w > 0.5) {
            float d = length(p - pr.xy);
            float r = max(pr.z, 1e-4);
            att = max(0.0, 1.0 - d / r);
        } else {
            // 简化的方向光：常量贡献（与 CPU 后备一致）
            att = 0.1;
        }
        float k = ci.a * att;
        add += ci.rgb * k;
    }

    add = clamp(add, 0.0, 1.0);
    vec3 rgb = base.rgb + add * (1.0 - base.rgb);
    gl_FragColor = vec4(clamp(rgb, 0.0, 1.0), base.a);
}

