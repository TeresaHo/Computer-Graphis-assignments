#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#include <GL/glew.h>
#include <freeglut/glut.h>
#include "textfile.h"
#include "glm.h"

#include "Texture.h"
#include "Matrices.h"

#define PI 3.1415926

#pragma comment (lib, "glew32.lib")
#pragma comment (lib, "freeglut.lib")

#ifndef GLUT_WHEEL_UP
# define GLUT_WHEEL_UP   0x0003
# define GLUT_WHEEL_DOWN 0x0004
#endif

#ifndef GLUT_KEY_ESC
# define GLUT_KEY_ESC 0x001B
#endif

#ifndef max
# define max(a,b) (((a)>(b))?(a):(b))
# define min(a,b) (((a)<(b))?(a):(b))
#endif

using namespace std;

struct iLocLightInfo
{
	GLuint position;
	GLuint ambient;
	GLuint diffuse;
	GLuint specular;
	GLuint spotDirection;
	GLuint spotCutoff;
	GLuint spotExponent;
	GLuint constantAttenuation;
	GLuint linearAttenuation;
	GLuint quadraticAttenuation;
}iLocLightInfo[2];

struct LightInfo
{
	Vector4 position;
	Vector4 spotDirection;
	Vector4 ambient;
	Vector4 diffuse;
	Vector4 specular;
	float spotExponent;
	float spotCutoff;
	float constantAttenuation;
	float linearAttenuation;
	float quadraticAttenuation;
}lightInfo[2];

struct Group
{
	int numTriangles;
	GLfloat *vertices;
	GLfloat *normals;

	// lighting parameter
	GLfloat ambient[4];
	GLfloat diffuse[4];
	GLfloat specular[4];
	GLfloat shininess;

	// texture
	unsigned char *image;			// Image data
	FileHeader fileHeader;			// File info of the texture image file
	InfoHeader infoHeader;			// Image info of the texture image
	GLfloat *texcoords;				// Texture coordinates
	GLuint texNum;					// The texture name(index) of this group's texture
};

struct Model
{
	int numGroups;

	GLMmodel *obj;
	Group *group;
	Matrix4 N;
	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	Vector3 rotation = Vector3(0, 0, 0);	// Euler form
};

struct Frustum
{
	float left, right, top, bottom, cnear, cfar;
};

struct Camera
{
	Vector3 position, center, upVector;
};

enum MainMode
{
	VertexLighting = 0,
	PixelLighting = 1,
	Geometric = 2,
	Viewing = 3,
	Texture = 4,
};

enum SubMode
{
	LightDirectional = 0,
	LightPoint = 1,
	GeoTranslation = 2,
	GeoRotation = 3,
	GeoScaling = 4,
	ViewCenter = 5,
	ViewEye = 6,
};

enum ProjMode
{
	Perspective = 0,
	Orthogonal = 1,
};

// Shader attributes
GLuint vaoHandler;
GLuint vertexVboHandler;
GLuint normalVboHandler;
GLuint texcoordsVboHandler;

// Shader attributes for uniform variables
GLuint iLocP, iLocV, iLocN, iLocR;
GLuint iLocLightTypePixel, iLocLightTypeVertex;
GLuint iLocKa, iLocKd, iLocKs;
GLuint iLocShininess;
GLuint iLocIsVertexLighting;
GLuint iLocTex;

Matrix4 M, V, P;

MainMode cur_main_mode = VertexLighting;
SubMode cur_sub_mode = LightDirectional;

int current_x, current_y;

Model* models;								// store the models we load
vector<string> filenames;					// .obj filename list
float scaleOffset = 0.8f;

int cur_idx = 0;							// represent which model should be rendered now

int lightTypePixel = 0;
int lightTypeVertex = 0;

Frustum frustum;
Camera camera;

int windowWidth = 1200;
int windowHeight = 600;

// Flags for texture parameters control
bool mag_linear = true;
bool min_linear = true;
bool warp_clamp = true;

Matrix4 myViewingMatrix()
{
	Vector3 X, Y, Z;
	Z = (camera.position - camera.center).normalize();
	Y = camera.upVector.normalize();
	X = Y.cross(Z).normalize();
	camera.upVector = Z.cross(X);

	Matrix4 mat = Matrix4(
		X.x, X.y, X.z, -X.dot(camera.position),
		Y.x, Y.y, Y.z, -Y.dot(camera.position),
		Z.x, Z.y, Z.z, -Z.dot(camera.position),
		0, 0, 0, 1
	);

	return mat;
}

Matrix4 myPerspective(Frustum frustum)
{
	GLfloat n = frustum.cnear;
	GLfloat f = frustum.cfar;
	GLfloat r = frustum.right;
	GLfloat l = frustum.left;
	GLfloat t = frustum.top;
	GLfloat b = frustum.bottom;

	Matrix4 mat = Matrix4(
		2 * n / (r - l), 0, (r + l) / (r - l), 0,
		0, 2 * n / (t - b), (t + b) / (t - b), 0,
		0, 0, -(f + n) / (f - n), -(2 * f*n) / (f - n),
		0, 0, -1, 0
	);

	return mat;
}

Matrix4 myOrthogonal(Frustum frustum)
{
	GLfloat tx = -(frustum.right + frustum.left) / (frustum.right - frustum.left);
	GLfloat ty = -(frustum.top + frustum.bottom) / (frustum.top - frustum.bottom);
	GLfloat tz = -(frustum.cfar + frustum.cnear) / (frustum.cfar - frustum.cnear);
	Matrix4 mat = Matrix4(
		2 / (frustum.right - frustum.left), 0, 0, tx,
		0, 2 / (frustum.top - frustum.bottom), 0, ty,
		0, 0, -2 / (frustum.cfar - frustum.cnear), tz,
		0, 0, 0, 1
	);

	return mat;
}

Matrix4 translate(Vector3 vec)
{
	Matrix4 mat;

	mat = Matrix4(
		1.0f, 0.0f, 0.0f, vec.x,
		0.0f, 1.0f, 0.0f, vec.y,
		0.0f, 0.0f, 1.0f, vec.z,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	return mat;
}

Matrix4 scaling(Vector3 vec)
{
	Matrix4 mat = Matrix4(
		vec.x, 0, 0, 0,
		0, vec.y, 0, 0,
		0, 0, vec.z, 0,
		0, 0, 0, 1
	);

	return mat;
}

Matrix4 rotateX(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, cosf(val), -sinf(val), 0.0f,
		0.0f, sinf(val), cosf(val), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	return mat;
}

Matrix4 rotateY(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		cosf(val), 0.0f, sinf(val), 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		-sinf(val), 0.0f, cosf(val), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	return mat;
}

Matrix4 rotateZ(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		cosf(val), -sinf(val), 0.0f, 0.0f,
		sinf(val), cosf(val), 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	return mat;
}

Matrix4 rotate(Vector3 vec)
{
	return rotateX(vec.x) * rotateY(vec.y) * rotateZ(vec.z);
}

void traverseColorModel(Model &m)
{
	m.numGroups = m.obj->numgroups;
	m.group = new Group[m.numGroups];
	GLMgroup* group = m.obj->groups;

	GLfloat maxVal[3] = { -100000, -100000, -100000 };
	GLfloat minVal[3] = { 100000, 100000, 100000 };

	int curGroupIdx = 0;

	// Iterate all the groups of this model
	while (group)
	{
		// If there exist material in this group
		if (strlen(m.obj->materials[group->material].textureImageName) != 0)
		{
			// Read the texture image from file
			string texName = "../../../TextureModels/" + string(m.obj->materials[group->material].textureImageName);

			FILE *f = fopen(texName.c_str(), "rb");
			fread(&m.group[curGroupIdx].fileHeader, sizeof(FileHeader), 1, f);
			fread(&m.group[curGroupIdx].infoHeader, sizeof(InfoHeader), 1, f);
			unsigned long size = m.group[curGroupIdx].infoHeader.Width * m.group[curGroupIdx].infoHeader.Height * 3;
			m.group[curGroupIdx].image = new unsigned char[size];
			//  Save texture image to model.group[group_idx].image
			fread(m.group[curGroupIdx].image, size * sizeof(char), 1, f);
			fclose(f);

			// [TODO] Fill in the blanks
			// Generate a texture and create mipmap
			glGenTextures(1, &m.group[curGroupIdx].texNum);
			glBindTexture(GL_TEXTURE_2D, m.group[curGroupIdx].texNum);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m.group[curGroupIdx].infoHeader.Width, m.group[curGroupIdx].infoHeader.Height, 0, GL_BGR, GL_UNSIGNED_BYTE, m.group[curGroupIdx].image);
			glGenerateMipmap(GL_TEXTURE_2D);
		}

		m.group[curGroupIdx].vertices = new GLfloat[group->numtriangles * 9];
		m.group[curGroupIdx].normals = new GLfloat[group->numtriangles * 9];
		m.group[curGroupIdx].texcoords = new GLfloat[group->numtriangles * 6];
		m.group[curGroupIdx].numTriangles = group->numtriangles;

		// Fetch material information
		memcpy(m.group[curGroupIdx].ambient, m.obj->materials[group->material].ambient, sizeof(GLfloat) * 4);
		memcpy(m.group[curGroupIdx].diffuse, m.obj->materials[group->material].diffuse, sizeof(GLfloat) * 4);
		memcpy(m.group[curGroupIdx].specular, m.obj->materials[group->material].specular, sizeof(GLfloat) * 4);

		m.group[curGroupIdx].shininess = m.obj->materials[group->material].shininess;

		// For each triangle in this group
		for (int i = 0; i < group->numtriangles; i++)
		{
			int triangleIdx = group->triangles[i];
			int indv[3] = {
				m.obj->triangles[triangleIdx].vindices[0],
				m.obj->triangles[triangleIdx].vindices[1],
				m.obj->triangles[triangleIdx].vindices[2]
			};
			int indn[3] = {
				m.obj->triangles[triangleIdx].nindices[0],
				m.obj->triangles[triangleIdx].nindices[1],
				m.obj->triangles[triangleIdx].nindices[2]
			};
			int indt[3] = {
				m.obj->triangles[triangleIdx].tindices[0],
				m.obj->triangles[triangleIdx].tindices[1],
				m.obj->triangles[triangleIdx].tindices[2]
			};

			// For each vertex in this triangle
			for (int j = 0; j < 3; j++)
			{
				m.group[curGroupIdx].vertices[i * 9 + j * 3 + 0] = m.obj->vertices[indv[j] * 3 + 0];
				m.group[curGroupIdx].vertices[i * 9 + j * 3 + 1] = m.obj->vertices[indv[j] * 3 + 1];
				m.group[curGroupIdx].vertices[i * 9 + j * 3 + 2] = m.obj->vertices[indv[j] * 3 + 2];

				m.group[curGroupIdx].normals[i * 9 + j * 3 + 0] = m.obj->normals[indn[j] * 3 + 0];
				m.group[curGroupIdx].normals[i * 9 + j * 3 + 1] = m.obj->normals[indn[j] * 3 + 1];
				m.group[curGroupIdx].normals[i * 9 + j * 3 + 2] = m.obj->normals[indn[j] * 3 + 2];

				m.group[curGroupIdx].texcoords[i * 6 + j * 2 + 0] = m.obj->texcoords[indt[j] * 2 + 0];
				m.group[curGroupIdx].texcoords[i * 6 + j * 2 + 1] = m.obj->texcoords[indt[j] * 2 + 1];

				maxVal[0] = max(maxVal[0], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 0]);
				maxVal[1] = max(maxVal[1], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 1]);
				maxVal[2] = max(maxVal[2], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 2]);

				minVal[0] = min(minVal[0], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 0]);
				minVal[1] = min(minVal[1], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 1]);
				minVal[2] = min(minVal[2], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 2]);
			}
		}
		group = group->next;
		curGroupIdx++;
	}

	// Create a normalize matrix of the model
	float norm_scale = max(max(abs(maxVal[0] - minVal[0]), abs(maxVal[1] - minVal[1])), abs(maxVal[2] - minVal[2]));
	Matrix4 S, T;

	S[0] = 2 / norm_scale * scaleOffset;	S[5] = 2 / norm_scale * scaleOffset;	S[10] = 2 / norm_scale * scaleOffset;
	T[3] = -(maxVal[0] + minVal[0]) / 2;
	T[7] = -(maxVal[1] + minVal[1]) / 2;
	T[11] = -(maxVal[2] + minVal[2]) / 2;
	m.N = S * T;
}

void loadOBJModel()
{
	models = new Model[filenames.size()];
	int idx = 0;
	for (string filename : filenames)
	{
		models[idx].obj = glmReadOBJ((char*)filename.c_str());
		traverseColorModel(models[idx++]);
	}
}

// [TODO] Fill in the blank
// Manually change texture parameters
void handleTexParameter()
{
	if (mag_linear)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	if (min_linear)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

	if (warp_clamp)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
}

void drawModel(Model &m)
{
	glBindVertexArray(vaoHandler);
	glUniform1i(iLocLightTypePixel, lightTypePixel);
	glUniform1i(iLocLightTypeVertex, lightTypeVertex);

	V = myViewingMatrix();

	handleTexParameter();

	for (int i = 0; i < m.numGroups; i++)
	{
		// set vertex and normal
		glBindBuffer(GL_ARRAY_BUFFER, vertexVboHandler);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float)*m.group[i].numTriangles * 9, m.group[i].vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, normalVboHandler);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float)*m.group[i].numTriangles * 9, m.group[i].normals, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, texcoordsVboHandler);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float)*m.group[i].numTriangles * 6, m.group[i].texcoords, GL_DYNAMIC_DRAW);

		// set light
		glUniform4fv(iLocKa, 1, m.group[i].ambient);
		glUniform4fv(iLocKd, 1, m.group[i].diffuse);
		glUniform4fv(iLocKs, 1, m.group[i].specular);
		glUniform1f(iLocShininess, m.group[i].shininess);

		// [TODO] Fill in the blank
		// Bind the texture
		glBindTexture(GL_TEXTURE_2D, m.group[i].texNum);

		// set mvp matrix
		Matrix4 T, R, S;
		T = translate(m.position);
		R = rotate(m.rotation);
		S = scaling(m.scale);
		M = T * R * S;

		glUniformMatrix4fv(iLocR, 1, GL_FALSE, M.getTranspose());
		glUniformMatrix4fv(iLocV, 1, GL_FALSE, V.getTranspose());
		glUniformMatrix4fv(iLocP, 1, GL_FALSE, P.getTranspose());
		glUniformMatrix4fv(iLocN, 1, GL_FALSE, m.N.getTranspose());

		// draw
		glDrawArrays(GL_TRIANGLES, 0, m.group[i].numTriangles * 3);
	}
}

void updateLight()
{
	glUniform4f(iLocLightInfo[0].position, lightInfo[0].position.x, lightInfo[0].position.y, lightInfo[0].position.z, lightInfo[0].position.w);
	glUniform4f(iLocLightInfo[0].ambient, lightInfo[0].ambient.x, lightInfo[0].ambient.y, lightInfo[0].ambient.z, lightInfo[0].ambient.w);
	glUniform4f(iLocLightInfo[0].diffuse, lightInfo[0].diffuse.x, lightInfo[0].diffuse.y, lightInfo[0].diffuse.z, lightInfo[0].diffuse.w);
	glUniform4f(iLocLightInfo[0].specular, lightInfo[0].specular.x, lightInfo[0].specular.y, lightInfo[0].specular.z, lightInfo[0].specular.w);

	glUniform4f(iLocLightInfo[1].position, lightInfo[1].position.x, lightInfo[1].position.y, lightInfo[1].position.z, lightInfo[1].position.w);
	glUniform4f(iLocLightInfo[1].ambient, lightInfo[1].ambient.x, lightInfo[1].ambient.y, lightInfo[1].ambient.z, lightInfo[1].ambient.w);
	glUniform4f(iLocLightInfo[1].diffuse, lightInfo[1].diffuse.x, lightInfo[1].diffuse.y, lightInfo[1].diffuse.z, lightInfo[1].diffuse.w);
	glUniform4f(iLocLightInfo[1].specular, lightInfo[1].specular.x, lightInfo[1].specular.y, lightInfo[1].specular.z, lightInfo[1].specular.w);
	glUniform1f(iLocLightInfo[1].constantAttenuation, lightInfo[1].constantAttenuation);
	glUniform1f(iLocLightInfo[1].linearAttenuation, lightInfo[1].linearAttenuation);
	glUniform1f(iLocLightInfo[1].quadraticAttenuation, lightInfo[1].quadraticAttenuation);
}

void showShaderCompileStatus(GLuint shader, GLint *shaderCompiled)
{
	glGetShaderiv(shader, GL_COMPILE_STATUS, shaderCompiled);
	if (GL_FALSE == (*shaderCompiled))
	{
		GLint maxLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

		// The maxLength includes the NULL character.
		GLchar *errorLog = (GLchar*)malloc(sizeof(GLchar) * maxLength);
		glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);
		fprintf(stderr, "%s", errorLog);

		glDeleteShader(shader);
		free(errorLog);
	}
}

void setVertexArrayObject()
{
	// Create and setup the vertex array object
	glGenVertexArrays(1, &vaoHandler);
	glBindVertexArray(vaoHandler);

	// Enable the vertex attribute arrays
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	// Create and setup the vertex buffer object
	glGenBuffers(1, &vertexVboHandler);
	glGenBuffers(1, &normalVboHandler);
	glGenBuffers(1, &texcoordsVboHandler);

	// Map index 0 to the position buffer
	glBindBuffer(GL_ARRAY_BUFFER, vertexVboHandler);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	// Map index 1 to the normal buffer
	glBindBuffer(GL_ARRAY_BUFFER, normalVboHandler);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	// Map index 2 to the texcoords buffer
	glBindBuffer(GL_ARRAY_BUFFER, texcoordsVboHandler);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, NULL);
}

void setUniformVariables(GLuint program)
{
	iLocIsVertexLighting = glGetUniformLocation(program, "isVertexLighting");

	iLocP = glGetUniformLocation(program, "um4p");
	iLocV = glGetUniformLocation(program, "um4v");
	iLocN = glGetUniformLocation(program, "um4n");
	iLocR = glGetUniformLocation(program, "um4r");

	iLocLightTypePixel = glGetUniformLocation(program, "lightTypePixel");
	iLocLightTypeVertex = glGetUniformLocation(program, "lightTypeVertex");

	iLocKa = glGetUniformLocation(program, "material.Ka");
	iLocKd = glGetUniformLocation(program, "material.Kd");
	iLocKs = glGetUniformLocation(program, "material.Ks");
	iLocShininess = glGetUniformLocation(program, "material.shininess");

	iLocLightInfo[0].position = glGetUniformLocation(program, "light[0].position");
	iLocLightInfo[0].ambient = glGetUniformLocation(program, "light[0].La");
	iLocLightInfo[0].diffuse = glGetUniformLocation(program, "light[0].Ld");
	iLocLightInfo[0].specular = glGetUniformLocation(program, "light[0].Ls");
	iLocLightInfo[0].spotDirection = glGetUniformLocation(program, "light[0].spotDirection");
	iLocLightInfo[0].spotCutoff = glGetUniformLocation(program, "light[0].spotCutoff");
	iLocLightInfo[0].spotExponent = glGetUniformLocation(program, "light[0].spotExponent");
	iLocLightInfo[0].constantAttenuation = glGetUniformLocation(program, "light[0].constantAttenuation");
	iLocLightInfo[0].linearAttenuation = glGetUniformLocation(program, "light[0].linearAttenuation");
	iLocLightInfo[0].quadraticAttenuation = glGetUniformLocation(program, "light[0].quadraticAttenuation");

	iLocLightInfo[1].position = glGetUniformLocation(program, "light[1].position");
	iLocLightInfo[1].ambient = glGetUniformLocation(program, "light[1].La");
	iLocLightInfo[1].diffuse = glGetUniformLocation(program, "light[1].Ld");
	iLocLightInfo[1].specular = glGetUniformLocation(program, "light[1].Ls");
	iLocLightInfo[1].spotDirection = glGetUniformLocation(program, "light[1].spotDirection");
	iLocLightInfo[1].spotCutoff = glGetUniformLocation(program, "light[1].spotCutoff");
	iLocLightInfo[1].spotExponent = glGetUniformLocation(program, "light[1].spotExponent");
	iLocLightInfo[1].constantAttenuation = glGetUniformLocation(program, "light[1].constantAttenuation");
	iLocLightInfo[1].linearAttenuation = glGetUniformLocation(program, "light[1].linearAttenuation");
	iLocLightInfo[1].quadraticAttenuation = glGetUniformLocation(program, "light[1].quadraticAttenuation");

	// [TODO] Fill in the blank
	// Get the location of the texture uniform variable
	iLocTex = glGetUniformLocation(program, "tex");
}

//load Shader
const char** loadShaderSource(const char* file)
{
	FILE* fp = fopen(file, "rb");
	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *src = new char[sz + 1];
	fread(src, sizeof(char), sz, fp);
	src[sz] = '\0';
	const char **srcp = new const char*[1];
	srcp[0] = src;
	return srcp;
}

void setShaders()
{
	GLuint vertexShader, fragmentShader, shaderProgram;
	const char **vs = NULL;
	const char **fs = NULL;

	vertexShader = glCreateShader(GL_VERTEX_SHADER);
	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	vs = loadShaderSource("shader.vert");
	fs = loadShaderSource("shader.frag");

	glShaderSource(vertexShader, 1, vs, NULL);
	glShaderSource(fragmentShader, 1, fs, NULL);

	free(vs);
	free(fs);

	// compile vertex shader
	glCompileShader(vertexShader);
	GLint vShaderCompiled;
	showShaderCompileStatus(vertexShader, &vShaderCompiled);
	if (!vShaderCompiled) system("pause"), exit(-1);

	// compile fragment shader
	glCompileShader(fragmentShader);
	GLint fShaderCompiled;
	showShaderCompileStatus(fragmentShader, &fShaderCompiled);
	if (!fShaderCompiled) system("pause"), exit(-1);

	shaderProgram = glCreateProgram();

	// bind shader
	glAttachShader(shaderProgram, fragmentShader);
	glAttachShader(shaderProgram, vertexShader);

	// link program
	glLinkProgram(shaderProgram);
	glUseProgram(shaderProgram);

	setUniformVariables(shaderProgram);
	setVertexArrayObject();
}

void onDisplay(void)
{
	// clear canvas
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);	// clear canvas to color(0,0,0)->black
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	updateLight();
	glUniform1i(iLocIsVertexLighting, 1);
	glViewport(0, 0, windowWidth / 2, windowHeight);
	drawModel(models[cur_idx]);

	glUniform1i(iLocIsVertexLighting, 0);
	glViewport(windowWidth / 2, 0, windowWidth / 2, windowHeight);
	drawModel(models[cur_idx]);

	glutSwapBuffers();
}

void onMouse(int who, int state, int x, int y)
{
	printf("%18s(): (%d, %d) ", __FUNCTION__, x, y);

	current_x = x;
	current_y = y;

	switch (cur_sub_mode)
	{
	case LightDirectional:
		lightInfo[0].position.x = ((double)x / windowWidth - 0.5) * 20;
		lightInfo[0].position.y = -((double)y / windowHeight - 0.5) * 10;
		break;
	case LightPoint:
		lightInfo[1].position.x = ((double)x / windowWidth - 0.5) * 10;
		lightInfo[1].position.y = -((double)y / windowHeight - 0.5) * 5;
		break;
	}

	switch (who)
	{
	case GLUT_LEFT_BUTTON:
		printf("left button   ");
		break;
	case GLUT_MIDDLE_BUTTON:
		printf("middle button "); break;
	case GLUT_RIGHT_BUTTON:
		printf("right button  ");
		break;
	case GLUT_WHEEL_UP:
		printf("wheel up      \n");
		break;
	case GLUT_WHEEL_DOWN:
		printf("wheel down    \n");
		break;
	default:
		printf("0x%02X          ", who);
		break;
	}

	switch (state)
	{
	case GLUT_DOWN: printf("start "); break;
	case GLUT_UP:   printf("end   "); break;
	}

	printf("\n");
}

void onMouseMotion(int x, int y)
{
	int diff_x = x - current_x;
	int diff_y = y - current_y;
	current_x = x;
	current_y = y;

	switch (cur_sub_mode)
	{
	case LightDirectional:
		lightInfo[0].position.x = ((double)x / windowWidth - 0.5) * 20;
		lightInfo[0].position.y = -((double)y / windowHeight - 0.5) * 10;
		break;
	case LightPoint:
		lightInfo[1].position.x = ((double)x / windowWidth - 0.5) * 10;
		lightInfo[1].position.y = -((double)y / windowHeight - 0.5) * 5;
		break;
	case GeoTranslation:
		models[cur_idx].position.x += diff_x * (1.0 / 400.0);
		models[cur_idx].position.y -= diff_y * (1.0 / 400.0);
		break;
	case GeoRotation:
		models[cur_idx].rotation.x += PI / 180.0 * diff_y * (45.0 / 400.0);
		models[cur_idx].rotation.y += PI / 180.0 * diff_x * (45.0 / 400.0);
		break;
	case GeoScaling:
		models[cur_idx].scale.x += diff_x * 0.01;
		models[cur_idx].scale.y += diff_y * 0.01;
		break;
	case ViewEye:
		camera.position.x += diff_x * (1.0 / 400.0);
		camera.position.y += diff_y * (1.0 / 400.0);
		break;
	case ViewCenter:
		camera.center.x += diff_x * (1.0 / 400.0);
		camera.center.y -= diff_y * (1.0 / 400.0);
		break;
	}

	printf("%18s(): (%d, %d) mouse move\n", __FUNCTION__, x, y);
}

void onKeyboard(unsigned char key, int x, int y)
{
	printf("%18s(): (%d, %d) key: %c(0x%02X) ", __FUNCTION__, x, y, key, key);
	switch (key)
	{
	case GLUT_KEY_ESC: /* the Esc key */
		exit(0);
		break;
	case 'z':
		cur_idx = (cur_idx + filenames.size() - 1) % filenames.size();
		break;
	case 'x':
		cur_idx = (cur_idx + 1) % filenames.size();
		break;
	case '1':
		cout << endl << "VertexLighting" << endl;
		cur_main_mode = VertexLighting;	
		break;
	case '2':
		cout << endl << "PixelLighting" << endl;
		cur_main_mode = PixelLighting;		
		break;
	case '3':
		cout << endl << "Geometric" << endl;
		cur_main_mode = Geometric;		
		break;
	case '4':
		cout << endl << "Viewing" << endl;
		cur_main_mode = Viewing;
		break;
	case '5':
		cout << endl << "Texture" << endl;
		cur_main_mode = Texture;
		break;
	case 'q':
		if (cur_main_mode == VertexLighting || cur_main_mode == PixelLighting) {
			cur_sub_mode = LightDirectional;
			if (cur_main_mode == VertexLighting)
				lightTypeVertex = 0;
			else
				lightTypePixel = 0;
		}
		else if (cur_main_mode == Geometric) {
			cur_sub_mode = GeoTranslation;
		}
		else if (cur_main_mode == Viewing) {
			cur_sub_mode = ViewCenter;
		}
		else if (cur_main_mode == Texture) {
			mag_linear = !mag_linear;
		}
		break;
	case 'w':
		if (cur_main_mode == VertexLighting || cur_main_mode == PixelLighting) {
			cur_sub_mode = LightPoint;
			if (cur_main_mode == VertexLighting)
				lightTypeVertex = 1;
			else
				lightTypePixel = 1;
		}
		else if (cur_main_mode == Geometric) {
			cur_sub_mode = GeoRotation;
		}
		else if (cur_main_mode == Viewing) {
			cur_sub_mode = ViewEye;
		}
		else if (cur_main_mode == Texture) {
			min_linear = !min_linear;
		}
		break;
	case 'e':
		if (cur_main_mode == Geometric) cur_sub_mode = GeoScaling;
		else if (cur_main_mode == Texture) warp_clamp = !warp_clamp;
		break;
	case 'o':
		P = myOrthogonal(frustum);
		break;
	case 'p':
		P = myPerspective(frustum);
		break;
	case 'h':
		//[TODO]show information
		//e.g: current mode, model index, control instructions
		//model info, camera info, lighting info, texture parameter
		cout << endl << "Information : " << endl;
		if (cur_main_mode == VertexLighting) {
			cout << "VertexLighting ( Mode 1 )" << endl;
			cout << " Q : DIRECTIONAL LGIHT" << endl;
			cout << " W : POINT LIGHT" << endl;
			cout << " E : NO FUNCTION" << endl;
		}
		else if (cur_main_mode == PixelLighting) {
			cout << "PixelLighting ( Mode 2 )" << endl;
			cout << " Q : DIRECTIONAL LIGHT" << endl;
			cout << " W : POINT LIGHT" << endl;
			cout << " E : NO FUNCTION" << endl;
		}
		else if (cur_main_mode == Geometric) {
			cout << "Geometric ( Mode 3 )" << endl;
			cout << " Q : TRANSLATION" << endl;
			cout << " W : ROTATION" << endl;
			cout << " E : SCALING" << endl;
		}
		else if (cur_main_mode == Viewing) {
			cout << "Viewing ( Mode 4 )" << endl;
			cout << " Q : VIEWCENTER" << endl;
			cout << " W : VIEWEYE" << endl;
			cout << " E : NO FUNCTION" << endl;
		}
		else if (cur_main_mode == Texture) {
			cout << " Texture( Mode 5 )" << endl;
			cout << " Q : MAGLINEAR" << endl;
			cout << " W : MINLINEAR" << endl;
			cout << " E : WARPCLAMP" << endl;
		}

		cout << "Camera INFO : " << endl;
		cout << "Camera : ( " << camera.position.x << " , " << camera.position.y << " )" << endl;

		cout << "Light INFO : " << endl;
		cout << "Light direction : ( " << lightInfo[0].position.x << " , " << lightInfo[0].position.y << " )" << endl;
		cout << "Light point : ( " << lightInfo[1].position.y << " , " << lightInfo[1].position.y << " )" << endl;
		cout << endl;

		break;
	}
	printf("\n");
}

void onWindowReshape(int width, int height)
{
	windowWidth = width;
	windowHeight = height;
	printf("%18s(): %dx%d\n", __FUNCTION__, width, height);
}

void onIdle()
{
	glutPostRedisplay();
}

void initParameter()
{
	camera.position = Vector3(0.0f, 0.0f, 3.0f);
	camera.center = Vector3(0.0f, 0.0f, 0.0f);
	camera.upVector = Vector3(0.0f, 1.0f, 0.0f);

	frustum.left = -1.0f;
	frustum.right = 1.0f;
	frustum.top = 1.0f;
	frustum.bottom = -1.0f;
	frustum.cnear = 1.0f;
	frustum.cfar = 1000.0f;
	P = myOrthogonal(frustum);

	lightInfo[0].position = Vector4(3.0f, 3.0f, 3.0f, 0.0f);
	lightInfo[0].ambient = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	lightInfo[0].diffuse = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	lightInfo[0].specular = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	lightInfo[1].position = Vector4(0.0f, -2.0f, 1.0f, 1.0f);
	lightInfo[1].ambient = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	lightInfo[1].diffuse = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	lightInfo[1].specular = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	lightInfo[1].constantAttenuation = 0.05;
	lightInfo[1].linearAttenuation = 0.3;
	lightInfo[1].quadraticAttenuation = 0.6f;
}

void loadConfigFile()
{
	ifstream fin;
	string line;
	fin.open("../../config.txt", ios::in);
	if (fin.is_open())
	{
		while (getline(fin, line))
		{
			filenames.push_back(line);
		}
		fin.close();
	}
	else
	{
		cout << "Unable to open the config file!" << endl;
	}
	for (int i = 0; i < filenames.size(); i++)
		printf("%s\n", filenames[i].c_str());
}

int main(int argc, char **argv)
{
	loadConfigFile();

	// glut init
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);

	// create window
	glutInitWindowPosition(500, 100);
	glutInitWindowSize(windowWidth, windowHeight);
	glutCreateWindow("CG HW4");

	glewInit();
	if (glewIsSupported("GL_VERSION_4_3"))
	{
		printf("Ready for OpenGL 4.3\n");
	}
	else
	{
		printf("OpenGL 4.3 not supported\n");
		system("pause");
		exit(1);
	}

	initParameter();

	// load obj models through glm
	loadOBJModel();

	// register glut callback functions
	glutDisplayFunc(onDisplay);
	glutReshapeFunc(onWindowReshape);
	glutIdleFunc(onIdle);
	glutKeyboardFunc(onKeyboard);
	glutMouseFunc(onMouse);
	glutMotionFunc(onMouseMotion);

	// set up shaders
	setShaders();

	glEnable(GL_DEPTH_TEST);

	// main loop
	glutMainLoop();

	// delete glm objects before exit
	for (int i = 0; i < filenames.size(); i++)
		glmDelete(models[i].obj);

	return 0;
}