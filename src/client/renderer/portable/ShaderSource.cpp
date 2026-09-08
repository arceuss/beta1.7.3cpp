#include "client/renderer/portable/ShaderSource.h"

#include <stdexcept>

namespace b173
{
namespace render
{
static const char *vertexBody = R"SHADER(
F3 unitVector(F3 v)
{
    float d=dot(v,v);
    return d>0.0 ? v*inversesqrt(d) : F3(0.0,0.0,0.0);
}
F4 transform4(F4 a,F4 b,F4 c,F4 d,F4 p)
{
    return a*p.x+b*p.y+c*p.z+d*p.w;
}
void legacyVertex(F3 position,F2 uv,F4 color,F4 packedNormal,
    out F4 clipPosition,out F4 primary,out F3 texCoord,out float fogDistance)
{
    F4 eye=transform4(uV[0],uV[1],uV[2],uV[3],F4(position,1.0));
    clipPosition=transform4(uV[4],uV[5],uV[6],uV[7],eye);
    // Logical OpenGL depth is converted exactly once. Direct3D's viewport already
    // maps +Y upward; Vulkan's positive-height viewport needs this shader Y flip.
    if(uV[30].x>0.5)clipPosition.z=(clipPosition.z+clipPosition.w)*0.5;
    if(uV[30].y>0.5)clipPosition.y=-clipPosition.y;
    F4 vertexColor=uV[19].y>0.5?color:uV[15];
    F3 normal=uV[16].xyz;
    if(uV[19].z>0.5)
    {
        F3 b=floor(packedNormal.xyz*255.0+0.5);
        b-=step(F3(128.0,128.0,128.0),b)*256.0;
        normal=uV[18].w>0.5?max(b/127.0,F3(-1.0,-1.0,-1.0)):(2.0*b+1.0)/255.0;
    }
    primary=vertexColor;
    if(uV[18].x>0.5)
    {
        normal=F3(dot(uV[12].xyz,normal),dot(uV[13].xyz,normal),dot(uV[14].xyz,normal))*uV[18].z;
        if(uV[18].y>0.5)normal=unitVector(normal);
        // glColorMaterial is per material term: uV[31].x enables tracking, .y is
        // the GL_AMBIENT mask and .z the GL_DIFFUSE mask. GL_AMBIENT alone must
        // leave the diffuse material, and therefore the lit alpha, untouched.
        F4 ma=(uV[31].x*uV[31].y)>0.5?vertexColor:uV[21];
        F4 md=(uV[31].x*uV[31].z)>0.5?vertexColor:uV[22];
        F3 lit=uV[23].xyz+uV[20].xyz*ma.xyz;
        lit+=uV[24].w*(uV[26].xyz*ma.xyz+uV[25].xyz*md.xyz*max(dot(normal,unitVector(uV[24].xyz)),0.0));
        lit+=uV[27].w*(uV[29].xyz*ma.xyz+uV[28].xyz*md.xyz*max(dot(normal,unitVector(uV[27].xyz)),0.0));
        primary=F4(lit,md.w);
    }
    // Fixed-function primary color is clamped per vertex, before interpolation.
    primary=clamp(primary,0.0,1.0);
    F4 tc=uV[19].x>0.5?F4(uv,0.0,1.0):uV[17];
    tc=transform4(uV[8],uV[9],uV[10],uV[11],tc);
    texCoord=F3(tc.x,tc.y,tc.w);
    fogDistance=uV[30].z>0.5?length(eye.xyz):abs(eye.z);
}
)SHADER";
static const char *fragmentBody = R"SHADER(
bool compareAlpha(float a,float operation,float r)
{
    if(operation<0.5)return false;
    if(operation<1.5)return a<r;
    if(operation<2.5)return a==r;
    if(operation<3.5)return a<=r;
    if(operation<4.5)return a>r;
    if(operation<5.5)return a!=r;
    if(operation<6.5)return a>=r;
    return true;
}
F4 wrappedTexel(F2 index,F2 size,F2 wrapAxes)
{
    // index counts whole texels. GLSL mod and HLSL fmod disagree on negatives,
    // so the repeat is written out; unflagged axes stay on the sampler's edge.
    F2 repeated=index-floor(index/size)*size;
    return sampleTexture((mix(index,repeated,wrapAxes)+0.5)/size);
}
F4 legacySample(F2 uv)
{
    // GL_CLAMP means clamp coordinates to [0,1], then include border texels in
    // linear filtering. EDGE is not equivalent. The backend sampler uses EDGE
    // for these axes; coverage restores the missing border contribution.
    // uF[4].zw flags axes whose GL_REPEAT the backend could not give the sampler
    // (ES 2.0 core rejects NPOT repeat), so this function repeats them by hand.
    F2 clampAxes=uF[4].xy;
    F2 wrapAxes=uF[4].zw;
    F2 size=uF[3].xy;
    F2 clamped=clamp(uv,0.0,1.0);
    F2 p=mix(uv,clamped,clampAxes);
    F4 texel;
    if(uF[3].z>0.5)
    {
        if(wrapAxes.x+wrapAxes.y>0.5)
        {
            // Reconstruct the bilinear filter from four texel centers so that a
            // repeated seam blends the opposite edge instead of the clamped one.
            F2 q=p*size-0.5;
            F2 base=floor(q);
            F2 f=q-base;
            F4 t00=wrappedTexel(base,size,wrapAxes);
            F4 t10=wrappedTexel(base+F2(1.0,0.0),size,wrapAxes);
            F4 t01=wrappedTexel(base+F2(0.0,1.0),size,wrapAxes);
            F4 t11=wrappedTexel(base+F2(1.0,1.0),size,wrapAxes);
            texel=mix(mix(t00,t10,f.x),mix(t01,t11,f.x),f.y);
        }
        else texel=sampleTexture(p);
        F2 coverage=clamp(min(clamped*size+0.5,(1.0-clamped)*size+0.5),0.0,1.0);
        coverage=mix(F2(1.0,1.0),coverage,clampAxes);
        texel=mix(uF[5],texel,coverage.x*coverage.y);
    }
    else texel=sampleTexture(mix(p,fract(p),wrapAxes));
    return texel;
}
F4 legacyFragment(F4 primary,F3 texCoord,float fogDistance)
{
    F4 result=primary;
    if(uF[2].x>0.5)
    {
        F4 t=legacySample(texCoord.xy/texCoord.z);
        float mode=uF[2].w;
        if(mode<0.5)result=primary*t;
        else if(mode<1.5)result=t;
        else if(mode<2.5)result=F4(mix(primary.xyz,t.xyz,t.w),primary.w);
        else if(mode<3.5)result=F4(primary.xyz+t.xyz,primary.w*t.w);
        else result=F4(primary.xyz*(1.0-t.xyz)+uF[6].xyz*t.xyz,primary.w*t.w);
    }
    result=clamp(result,0.0,1.0);
    if(!compareAlpha(result.w,uF[2].y,uF[2].z))discard;
    float fogMode=uF[1].w;
    if(fogMode>0.5)
    {
        float f=1.0;
        if(fogMode<1.5)f=(uF[1].y-fogDistance)/(uF[1].y-uF[1].x);
        else if(fogMode<2.5)f=exp(-uF[1].z*fogDistance);
        else {float d=uF[1].z*fogDistance;f=exp(-d*d);}
        result.xyz=mix(uF[0].xyz,result.xyz,clamp(f,0.0,1.0));
    }
    return result;
}
)SHADER";

static std::string glslPrelude(ShaderLanguage lang, bool highp, bool vertex)
{
	std::string s;
	if (lang == ShaderLanguage::GL330)
	{
		s = "#version 330 core\n";
	}
	else if (lang == ShaderLanguage::ES100)
	{
		s = "#version 100\n";
		// ES 2.0 always guarantees vertex highp; only the fragment stage may lack it.
		s += vertex || highp ? "precision highp float;\n" : "precision mediump float;\n";
		s += "precision mediump int;\n";
	}
	else if (lang == ShaderLanguage::Vulkan450)
	{
		s = "#version 450\n";
	}
	else
	{
		throw std::invalid_argument("not a GLSL dialect");
	}
	s += "#define F2 vec2\n#define F3 vec3\n#define F4 vec4\n";
	if (lang == ShaderLanguage::Vulkan450)
		s += "layout(std140,set=0,binding=0) uniform LegacyConstants { vec4 uV[32]; vec4 uF[8]; };\n";
	else
		s += vertex ? "uniform vec4 uV[32];\n" : "uniform vec4 uF[8];\n";
	return s;
}

std::string vertexShader(ShaderLanguage lang, bool highp)
{
	if (lang == ShaderLanguage::HLSL50)
		return hlslShader();
	std::string s = glslPrelude(lang, highp, true);
	if (lang == ShaderLanguage::ES100)
	{
		s += "attribute vec3 aPosition;\nattribute vec2 aUV;\nattribute vec4 aColor;\nattribute vec4 aNormal;\n";
		const std::string precision = highp ? "highp" : "mediump";
		s += "varying " + precision + " vec4 vPrimary;\nvarying " + precision + " vec3 vTexCoord;\nvarying " +
			 precision + " float vFog;\n";
	}
	else
	{
		s += "layout(location=0) in vec3 aPosition;\nlayout(location=1) in vec2 aUV;\nlayout(location=2) in vec4 "
			 "aColor;\nlayout(location=3) in vec4 aNormal;\n";
		if (lang == ShaderLanguage::Vulkan450)
			s += "layout(location=0) out vec4 vPrimary;\nlayout(location=1) out vec3 vTexCoord;\nlayout(location=2) "
				 "out float vFog;\n";
		else
			s += "out vec4 vPrimary;\nout vec3 vTexCoord;\nout float vFog;\n";
	}
	s += vertexBody;
	s += "void "
		 "main(){legacyVertex(aPosition,aUV,aColor,aNormal,gl_Position,vPrimary,vTexCoord,vFog);gl_PointSize=1.0;}\n";
	return s;
}

std::string fragmentShader(ShaderLanguage lang, bool highp)
{
	if (lang == ShaderLanguage::HLSL50)
		return hlslShader();
	std::string s = glslPrelude(lang, highp, false);
	if (lang == ShaderLanguage::Vulkan450)
	{
		s += "layout(set=0,binding=1) uniform sampler2D uTexture;\nlayout(location=0) in vec4 "
			 "vPrimary;\nlayout(location=1) in vec3 vTexCoord;\nlayout(location=2) in float vFog;\nlayout(location=0) "
			 "out vec4 outColor;\n";
	}
	else if (lang == ShaderLanguage::GL330)
	{
		s += "uniform sampler2D uTexture;\nin vec4 vPrimary;\nin vec3 vTexCoord;\nin float vFog;\nlayout(location=0) "
			 "out vec4 outColor;\n";
	}
	else
	{
		const std::string precision = highp ? "highp" : "mediump";
		s += "uniform " + precision + " sampler2D uTexture;\nvarying " + precision + " vec4 vPrimary;\nvarying " +
			 precision + " vec3 vTexCoord;\nvarying " + precision + " float vFog;\n";
	}
	s += lang == ShaderLanguage::ES100 ? "vec4 sampleTexture(vec2 p){return texture2D(uTexture,p);}\n"
									   : "vec4 sampleTexture(vec2 p){return texture(uTexture,p);}\n";
	s += fragmentBody;
	s += lang == ShaderLanguage::ES100 ? "void main(){gl_FragColor=legacyFragment(vPrimary,vTexCoord,vFog);}\n"
									   : "void main(){outColor=legacyFragment(vPrimary,vTexCoord,vFog);}\n";
	return s;
}

std::string hlslShader()
{
	std::string s = R"SHADER(
#define F2 float2
#define F3 float3
#define F4 float4
#define inversesqrt rsqrt
#define mix lerp
#define fract frac
cbuffer LegacyConstants : register(b0) { float4 uV[32]; float4 uF[8]; };
Texture2D<float4> uTexture : register(t0);
SamplerState uSampler : register(s0);
struct VSIn { float3 position:POSITION;float2 uv:TEXCOORD0;float4 color:COLOR0;float4 normal:NORMAL0; };
struct VSOut { float4 position:SV_Position;float4 color:COLOR0;float3 tc:TEXCOORD0;float fog:TEXCOORD1; };
float4 sampleTexture(float2 p){return uTexture.Sample(uSampler,p);}
)SHADER";
	s += vertexBody;
	s += fragmentBody;
	s += R"SHADER(
VSOut vsMain(VSIn i)
{
    VSOut o;legacyVertex(i.position,i.uv,i.color,i.normal,o.position,o.color,o.tc,o.fog);return o;
}
float4 psMain(VSOut i):SV_Target0{return legacyFragment(i.color,i.tc,i.fog);}
)SHADER";
	return s;
}
} // namespace render
} // namespace b173
