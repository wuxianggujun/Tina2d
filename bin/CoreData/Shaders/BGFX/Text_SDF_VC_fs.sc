$input v_color0, v_texcoord0

#include "bgfx_shader.sh"

// 主纹理：Alpha 通道保存 SDF 距离
SAMPLER2D(s_texColor, 0);

// 可调 SDF 参数：x=edge（阈值，默认0.5），y=softnessScale（软化倍数，默认1.0）
uniform vec4 u_sdfParams;

void main()
{
    vec4 tex = texture2D(s_texColor, v_texcoord0);
    float dist = tex.a;
    // 基于屏幕导数的软化半径，乘以可调缩放
    float w = max(fwidth(dist), 1e-4) * max(u_sdfParams.y, 1e-4);
    float edge = (u_sdfParams.x > 0.0) ? u_sdfParams.x : 0.5;
    float t = saturate((dist - (edge - w)) / (2.0 * w));
    float alpha = t * v_color0.a;
    vec3 rgb = v_color0.rgb;
    gl_FragColor = vec4(rgb, alpha);
}
