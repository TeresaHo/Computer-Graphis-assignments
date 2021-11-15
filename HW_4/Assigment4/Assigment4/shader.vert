#version 430

layout (location = 0) in vec3 v_vertex;		// vertex coordinates in model space
layout (location = 1) in vec3 v_normal;		// vertex normal in model space
layout (location = 2) in vec2 v_texcoord;	// texture coordinates in model space

out vec3 f_vertexInView;	// calculate lighting of vertex in camera space
out vec3 f_normalInView;	// calculate lighting of normal in camera space
out vec2 f_texcoord;
out vec4 V_color;

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

uniform mat4 um4p;	// projection matrix
uniform mat4 um4v;	// camera viewing transformation matrix
uniform mat4 um4n;	// model normalization matrix
uniform mat4 um4r;	// rotation matrix

uniform int lightTypeVertex;
uniform LightInfo light[2];
uniform MaterialInfo material;

vec4 directionalLight(vec3 N, vec3 V)
{
	vec4 lightInView = um4v * light[0].position;	// the position of the light in camera space
	vec3 S =  normalize(lightInView.xyz + V);		// Normalized lightInView

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
	float fp = 1 / (light[1].constantAttenuation + light[1].linearAttenuation * d + light[1].quadraticAttenuation * d * d);
	fp = min(fp, 1);
	
	vec3 H = normalize(S + V);

	vec4 I_ambient = light[1].La * material.Ka;
	vec4 I_diffuse = light[1].Ld * material.Kd * dot(N, S);
	vec4 I_specular = light[1].Ls * material.Ks * pow(dot(H, N), 64);

	return I_ambient + fp * I_diffuse + fp * I_specular;
}

// To simplify the calculation, we do calculation for lighting in camera space 
void main() 
{	
	vec4 vertexInView = um4v * um4r * um4n * vec4(v_vertex, 1.0);
	vec4 normalInView = transpose(inverse(um4v * um4r * um4n)) * vec4(v_normal, 0.0);

	f_vertexInView = vertexInView.xyz;
	f_normalInView = normalInView.xyz;

	vec3 N = normalize(normalInView.xyz);		// N represents normalized normal of the model in camera space
	vec3 V = -vertexInView.xyz;

	if(lightTypeVertex == 0)	
		V_color = directionalLight(N, V);	
	else if(lightTypeVertex == 1)	
		V_color = pointLight(N, V);
	
	f_texcoord = v_texcoord;					// pass texture coordinates to fragment shader
	gl_Position = um4p * um4v * um4r * um4n * vec4(v_vertex, 1.0);
}
