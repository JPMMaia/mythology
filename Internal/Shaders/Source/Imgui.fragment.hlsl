struct Pixel_shader_input
{
    float4 color : COLOR;
    float2 texture_coordinates : TEXCOORD;
};

struct Pixel_shader_output
{
	float4 color : SV_Target;
};

SamplerState const s_linear_wrap_sampler : register(s0, space7);

Texture2D g_texture : register(t0, space1);

Pixel_shader_output main(Pixel_shader_input const input)
{
    float4 const texture_color = g_texture.Sample(s_linear_wrap_sampler, input.texture_coordinates);

    Pixel_shader_output output;
	output.color = input.color * texture_color;

	return output;
}