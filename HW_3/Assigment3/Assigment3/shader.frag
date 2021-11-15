#version 430

in vec3 f_vertexInView;
in vec3 f_normalInView;
in vec4 V_color;

out vec4 fragColor;

struct LightInfo{
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

uniform int lightIdx;			// Use this variable to contrl lighting mode
uniform mat4 um4v;				// Camera viewing transformation matrix
uniform LightInfo light[3];
uniform MaterialInfo material;
uniform int vertex_or_perpixel;

vec4 directionalLight(vec3 N, vec3 V){

	vec4 lightInView = um4v * light[0].position;	// the position of the light in camera space
	vec3 S = normalize(lightInView.xyz + V);		 
	vec3 H = normalize(S + V);						// Half vector

	// [TODO] calculate diffuse coefficient and specular coefficient here
	float dc = max(dot(N, S), 0);	
	float sc = pow(max(dot(N, H), 0), 64); 

	return light[0].La * material.Ka + dc * light[0].Ld * material.Kd + sc * light[0].Ls * material.Ks;
}

vec4 pointLight(vec3 N, vec3 V){
	
	// [TODO] Calculate point light intensity here
	vec4 lightInView = um4v * light[1].position;	// the position of the light in camera space
	vec3 s = lightInView.xyz - f_vertexInView;
	vec3 S = normalize(s);			// Normalized lightInView = L
	vec3 H = normalize(S + V);						// Half vector

	float dc = max(dot(N, S), 0);	
	float sc = pow(max(dot(N, H), 0), 64); 
	float flatten = min(1/(light[1].constantAttenuation + light[1].linearAttenuation*length(S) + light[1].quadraticAttenuation*length(S)*length(S)), 1);

	return light[1].La * material.Ka + flatten * (dc * light[1].Ld * material.Kd + sc * light[1].Ls * material.Ks);

}

vec4 spotLight(vec3 N, vec3 V){
	
	//[TODO] Calculate spot light intensity here
	vec4 lightInView = um4v * light[2].position;	// the position of the light in camera space
	vec3 s = lightInView.xyz - f_vertexInView;
	vec3 S = normalize(s);			// Normalized lightInView = L
	vec3 H = normalize(S + V);						// Half vector

	//[TODO] Calculate spot light intensity here
	float dc = max(dot(N, S), 0);	
	float sc = pow(max(dot(N, H), 0), 64); 
	float flatten = min(1/(light[2].constantAttenuation + light[2].linearAttenuation*length(S) + light[2].quadraticAttenuation*length(S)*length(S)), 1);
	float spot_effect;
	
	if (dot(S, normalize(-light[2].spotDirection.xyz)) < cos(light[2].spotCutoff)) {
		spot_effect = 0;
	}
	else {
		spot_effect = pow(max(dot(S, normalize(-light[2].spotDirection.xyz)), 0), light[2].spotExponent);
	}
	
	return light[2].La * material.Ka + spot_effect * flatten * (dc * light[2].Ld * material.Kd + sc * light[2].Ls * material.Ks);

}

void main() {

	vec3 N = normalize(f_normalInView);		// N represents normalized normal of the model in camera space
	vec3 V = -f_vertexInView;	// V represents the vector from the vertex of the model to the camera position
	
	vec4 color = vec4(0, 0, 0, 0);

	// Handle lighting mode
	if(lightIdx == 0)
	{
		color += directionalLight(N, V);
	}
	else if(lightIdx == 1)
	{
		color += pointLight(N, V);
	}
	else if(lightIdx == 2)
	{
		color += spotLight(N ,V);
	}

	//[TODO] Use vertex_or_perpixel to decide which mode.
	if(vertex_or_perpixel==0){
		fragColor = color;
	}
	else{
		fragColor = V_color;
	}
	
}
