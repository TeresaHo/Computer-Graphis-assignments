#version 430

in vec3 f_vertexInView;
in vec3 f_normalInView;
in vec2 f_texcoord;
in vec4 V_color;

out vec4 fragColor;

struct LightInfo
{
	vec4 position;
	vec4 spotDirection;
	vec4 La;			// Ambient light intensity
	vec4 Ld;			// Diffuse light intensity
	vec4 Ls;			// Specular light intensity
	float spotExponent;
	float spotCutoff;
	float constantAttenuation;
	float linearAttenuation;
	float quadraticAttenuation;
};

struct MaterialInfo
{
	vec4 Ka;
	vec4 Kd;
	vec4 Ks;
	float shininess;
};

uniform int lightTypePixel;			// Use this variable to contrl lighting mode
uniform mat4 um4v;					// Camera viewing transformation matrix
uniform LightInfo light[2];
uniform MaterialInfo material;
uniform int isVertexLighting;

uniform sampler2D tex;				// texture uniform

vec4 directionalLight(vec3 N, vec3 V)
{
	vec4 lightInView = um4v * light[0].position;	// the position of the light in camera space
	vec3 S = normalize(lightInView.xyz + V);		// Normalized lightInView
	vec3 H = normalize(S + V);						// Half vector

	float dc = dot(N,S);	
	float sc = pow(max(dot(H, N), 0), 64);

	return light[0].La * material.Ka + dc * light[0].Ld * material.Kd + sc * light[0].Ls * material.Ks;
}

vec4 pointLight(vec3 N, vec3 V)
{	
	vec4 lightInView = um4v * light[1].position;	// the position of the light in camera space
	vec3 S = normalize(lightInView.xyz + V);

	float d = distance(-V, lightInView.xyz);
	float fp = 1/(light[1].constantAttenuation+light[1].linearAttenuation*d+light[1].quadraticAttenuation*d*d);
	fp = min(fp,1);
	
	vec3 H = normalize(S + V);
	vec4 I_ambient = light[1].La * material.Ka;
	vec4 I_diffuse = light[1].Ld * material.Kd * dot(N,S);
	vec4 I_specular = light[1].Ls * material.Ks * pow(dot(H,N),64);

	return I_ambient + fp * I_diffuse + fp * I_specular;
}

void main() 
{
	vec3 N = normalize(f_normalInView);							// N represents normalized normal of the model in camera space
	vec3 V = -f_vertexInView;									// V represents the vector from the vertex of the model to the camera position
	vec4 color = vec4(0, 0, 0, 0);

	// Handle lighting mode
	if(lightTypePixel == 0)	
		color = directionalLight(N, V);	
	else if(lightTypePixel == 1)	
		color = pointLight(N, V);

	if(isVertexLighting == 0) fragColor = V_color;
	else fragColor = color;
	
	// [TODO] Fill in the blank	
	vec4 texColor = vec4(texture( tex, f_texcoord).rgb, 1.0);
	fragColor = texColor*fragColor;
}