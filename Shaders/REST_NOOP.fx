#include "ReShade.fxh"

void Noop(in float4 pos : SV_Position, in float2 texcoord : Texcoord, out float4 o : SV_Target0)
{
    discard;
}

technique REST_NOOP
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = Noop; 
    }
}
