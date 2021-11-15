#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#include <GL/glew.h>
#include <freeglut/glut.h>
#include "glm.h"

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

// Shader attributes
GLint iLocPosition;
GLint iLocColor;
GLint iLocMVP;

struct camera
{
	Vector3 position;
	Vector3 center;
	Vector3 up_vector;
};

struct model
{
	GLMmodel *obj;
	GLfloat *vertices;
	GLfloat *colors;

	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	Vector3 rotation = Vector3(0, 0, 0);	// Euler form
};

struct project_setting
{
	GLfloat nearClip, farClip;
	GLfloat fovy;
	GLfloat aspect;
	GLfloat left, right, top, bottom;
};

enum TransMode
{
	GeoTranslation = 0,
	GeoRotation = 1,
	GeoScaling = 2,
	ViewCenter = 3,
	ViewEye = 4,
	ViewUp = 5,
};

enum ProjMode
{
	Orthogonal = 0,
	Perspective = 1,
};

Matrix4 view_matrix;
Matrix4 project_matrix;

project_setting proj;
camera main_camera;

int current_x, current_y;

model* models;							// store the models we load
vector<string> filenames;				// .obj filename list

int cur_idx = 0;						// represent which model should be rendered now
int pre_idx = 0;
int state = 0;
int anim = 0;
int start_pos = 0;


Vector3 translation = Vector3(0.0, 0.0, 0.0);
Vector3 scale = Vector3(0.0, 0.0, 0.0);
Vector3 rotation = Vector3(0.0, 0.0, 0.0);
Vector3 vieweye = Vector3(0.0, 0.0, 0.0);
Vector3 viewcenter = Vector3(0.0, 0.0, 0.0);
Vector3 viewup = Vector3(0.0, 0.0, 0.0);

bool use_wire_mode = false;
bool self_rotate = false;

TransMode cur_trans_mode = GeoTranslation;
ProjMode cur_proj_mode = Perspective;

// Normalize the given vector
static GLvoid glmNormalize(GLfloat v[3])
{
	GLfloat l;
	l = (GLfloat)sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] /= l;
	v[1] /= l;
	v[2] /= l;
}

// Caculate the cross product of u[3] and v[3] then output the result to n[3]
static GLvoid glmCross(GLfloat u[3], GLfloat v[3], GLfloat n[3])
{
	n[0] = u[1] * v[2] - u[2] * v[1];
	n[1] = u[2] * v[0] - u[0] * v[2];
	n[2] = u[0] * v[1] - u[1] * v[0];
}

// [TODO] given a translation vector then output a Matrix4 (Translation Matrix)
Matrix4 translate(Vector3 vec)
{
	Matrix4 mat;

	mat = Matrix4(  1.0, 0.0, 0.0, vec.x,
					0.0, 1.0, 0.0, vec.y,
					0.0, 0.0, 1.0, vec.z,
					0.0, 0.0, 0.0, 1.0);
	return mat;
}

// [TODO] given a scaling vector then output a Matrix4 (Scaling Matrix)
Matrix4 scaling(Vector3 vec)
{
	Matrix4 mat;

	mat = Matrix4(  vec.x, 0.0, 0.0, 0.0,
					0.0, vec.y, 0.0, 0.0,
					0.0, 0.0, vec.z, 0.0,
					0.0, 0.0, 0.0, 1.0 );

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-X (rotate alone axis-X)
Matrix4 rotateX(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(  1.0, 0.0, 0.0, 0.0,
					0.0, cos(val), -sin(val), 0.0,
					0.0, sin(val), cos(val), 0.0,
					0.0, 0.0, 0.0, 1.0);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-X (rotate alone axis-Y)
Matrix4 rotateY(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(  cos(val), 0.0, sin(val), 0.0,
					0.0, 1.0, 0.0, 0.0,
					-sin(val), 0.0, cos(val), 0.0,
					0.0, 0.0, 0.0, 1.0);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-X (rotate alone axis-Z)
Matrix4 rotateZ(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(  cos(val), -sin(val), 0.0, 0.0,
					sin(val), cos(val), 0.0, 0.0,
					0.0, 0.0, 1.0, 0.0,
					0.0, 0.0, 0.0, 1.0);

	return mat;
}

// Given a rotation vector then output the Matrix4 (Rotation Matrix)
Matrix4 rotate(Vector3 vec)
{
	return rotateX(vec.x)*rotateY(vec.y)*rotateZ(vec.z);
}

void setViewingMatrix()
{
	// [TODO] compute viewing matrix according to the setting of main_camera, then assign to view_matrix
	Vector3 VE = Vector3(main_camera.position.x, main_camera.position.y, main_camera.position.z);
	Vector3 VC = Vector3(main_camera.center.x, main_camera.center.y, main_camera.center.z);
	Vector3 VU = main_camera.up_vector;

	Vector3 p1_p2 = VC - VE;
	Vector3 p1_p3 = VU -VE;

	GLfloat r_Rz[] = {p1_p2.x, p1_p2.y, p1_p2.z};
	glmNormalize(r_Rz);
	Vector3 Rz = Vector3(-r_Rz[0], -r_Rz[1], -r_Rz[2]);
	GLfloat tmp[] = { p1_p2.x, p1_p2.y, p1_p2.z };
	GLfloat tmp1[] = { p1_p3.x, p1_p3.y, p1_p3.z };
	GLfloat r_Rx[3];
	glmCross(tmp, tmp1, r_Rx);
	glmNormalize(r_Rx);
	Vector3 Rx = Vector3(r_Rx[0], r_Rx[1], r_Rx[2]);

	GLfloat r_Ry[3];
	glmCross(r_Rz, r_Rx, r_Ry);
	Vector3 Ry = Vector3(r_Ry[0], r_Ry[1], r_Ry[2]);

	Matrix4 VR = Matrix4( Rx.x, Rx.y, Rx.z, 0.0,
					Ry.x, Ry.y, Ry.z, 0.0,
					Rz.x, Rz.y, Rz.z, 0.0,
					0.0, 0.0, 0.0, 1.0 );

	Matrix4 VT = Matrix4( 1.0, 0.0, 0.0, -main_camera.center.x,
					0.0, 1.0, 0.0, -main_camera.center.y,
					0.0, 0.0, 1.0, -main_camera.center.z,
					0.0, 0.0, 0.0, 1.0 );

	view_matrix = VR * VT;

}

void setOrthogonal()
{
	cur_proj_mode = Orthogonal;
	// [TODO] compute orthogonal projection matrix, then assign to project_matrix
	project_matrix = Matrix4( 2.0 / (proj.right - proj.left), 0.0, 0.0, -(proj.right + proj.left) / (proj.right - proj.left),
						0.0, 2.0 / (proj.top - proj.bottom), 0.0, -(proj.top + proj.bottom) / (proj.top - proj.bottom),
						0.0, 0.0, 2.0 / (proj.farClip - proj.nearClip), -(proj.farClip + proj.nearClip) / (proj.farClip - proj.nearClip),
						0.0, 0.0, 0.0, 1.0 );
}

void setPerspective()
{
	cur_proj_mode = Perspective;
	// [TODO] compute persepective projection matrix, then assign to project_matrix
	project_matrix = Matrix4(
		2 * proj.nearClip / (proj.right - proj.left), 0.0f, (proj.right + proj.left) / (proj.right - proj.left), 0.0f,
		0.0f, 2 * proj.nearClip / (proj.top - proj.bottom), (proj.top + proj.bottom) / (proj.top - proj.bottom), 0.0f,
		0.0f, 0.0f, 1 * (proj.farClip + proj.nearClip) / (proj.farClip - proj.nearClip), -2 * proj.farClip * proj.nearClip / (proj.farClip - proj.nearClip),
		0.0f, 0.0f, 1.0f, 0.0f
	);
}

void modelMovement(model *m, int i)
{
	// [TODO] Apply the movement on current frame
	if (anim==0)
	{
		if (i==cur_idx)
		{
			switch (cur_trans_mode) {
			case GeoTranslation:
				m->position += Vector3(translation.x / 50.0, -translation.y / 50.0, translation.z / 50.0);
				translation = Vector3(0.0, 0.0, 0.0);
				break;
			case GeoScaling:
				m->scale += Vector3(scale.x / 50.0, scale.y / 50.0, scale.z / 50.0);
				scale = Vector3(0.0, 0.0, 0.0);
				break;
			case GeoRotation:
				m->rotation += Vector3(rotation.x / 50.0, rotation.y / 50.0, rotation.z / 50.0);
				rotation = Vector3(0.0, 0.0, 0.0);
				break;
			}
		}
		switch (cur_trans_mode) {
			case ViewEye:
				main_camera.center += vieweye / 50.0;
				vieweye = Vector3(0.0, 0.0, 0.0);
				setViewingMatrix();
				break;
			case ViewCenter:
				main_camera.position += viewcenter / 50.0;
				viewcenter = Vector3(0.0, 0.0, 0.0);
				setViewingMatrix();
				break;
			case ViewUp:
				main_camera.up_vector += viewup / 50.0;
				viewup = Vector3(0.0, 0.0, 0.0);
				setViewingMatrix();
				break;

		}
	}
}

void traverseColorModel(model &m)
{
	GLfloat maxVal[3];
	GLfloat minVal[3];


	m.vertices = new GLfloat[m.obj->numtriangles * 9];
	m.colors = new GLfloat[m.obj->numtriangles * 9];

	// The center position of the model 
	m.obj->position[0] = 0;
	m.obj->position[1] = 0;
	m.obj->position[2] = 0;


	for (int i = 0; i < (int)m.obj->numtriangles; i++)
	{
		// the index of each vertex
		int indv1 = m.obj->triangles[i].vindices[0];
		int indv2 = m.obj->triangles[i].vindices[1];
		int indv3 = m.obj->triangles[i].vindices[2];

		// the index of each color
		int indc1 = indv1;
		int indc2 = indv2;
		int indc3 = indv3;

		// assign vertices
		GLfloat vx, vy, vz;
		vx = m.obj->vertices[indv1 * 3 + 0];
		vy = m.obj->vertices[indv1 * 3 + 1];
		vz = m.obj->vertices[indv1 * 3 + 2];

		m.vertices[i * 9 + 0] = vx;
		m.vertices[i * 9 + 1] = vy;
		m.vertices[i * 9 + 2] = vz;

		vx = m.obj->vertices[indv2 * 3 + 0];
		vy = m.obj->vertices[indv2 * 3 + 1];
		vz = m.obj->vertices[indv2 * 3 + 2];

		m.vertices[i * 9 + 3] = vx;
		m.vertices[i * 9 + 4] = vy;
		m.vertices[i * 9 + 5] = vz;

		vx = m.obj->vertices[indv3 * 3 + 0];
		vy = m.obj->vertices[indv3 * 3 + 1];
		vz = m.obj->vertices[indv3 * 3 + 2];

		m.vertices[i * 9 + 6] = vx;
		m.vertices[i * 9 + 7] = vy;
		m.vertices[i * 9 + 8] = vz;

		// assign colors
		GLfloat c1, c2, c3;
		c1 = m.obj->colors[indv1 * 3 + 0];
		c2 = m.obj->colors[indv1 * 3 + 1];
		c3 = m.obj->colors[indv1 * 3 + 2];

		m.colors[i * 9 + 0] = c1;
		m.colors[i * 9 + 1] = c2;
		m.colors[i * 9 + 2] = c3;

		c1 = m.obj->colors[indv2 * 3 + 0];
		c2 = m.obj->colors[indv2 * 3 + 1];
		c3 = m.obj->colors[indv2 * 3 + 2];

		m.colors[i * 9 + 3] = c1;
		m.colors[i * 9 + 4] = c2;
		m.colors[i * 9 + 5] = c3;

		c1 = m.obj->colors[indv3 * 3 + 0];
		c2 = m.obj->colors[indv3 * 3 + 1];
		c3 = m.obj->colors[indv3 * 3 + 2];

		m.colors[i * 9 + 6] = c1;
		m.colors[i * 9 + 7] = c2;
		m.colors[i * 9 + 8] = c3;
	}
	// Find min and max value
	GLfloat meanVal[3];

	meanVal[0] = meanVal[1] = meanVal[2] = 0;
	maxVal[0] = maxVal[1] = maxVal[2] = -10e20;
	minVal[0] = minVal[1] = minVal[2] = 10e20;

	for (int i = 0; i < m.obj->numtriangles * 3; i++)
	{
		maxVal[0] = max(m.vertices[3 * i + 0], maxVal[0]);
		maxVal[1] = max(m.vertices[3 * i + 1], maxVal[1]);
		maxVal[2] = max(m.vertices[3 * i + 2], maxVal[2]);

		minVal[0] = min(m.vertices[3 * i + 0], minVal[0]);
		minVal[1] = min(m.vertices[3 * i + 1], minVal[1]);
		minVal[2] = min(m.vertices[3 * i + 2], minVal[2]);

		meanVal[0] += m.vertices[3 * i + 0];
		meanVal[1] += m.vertices[3 * i + 1];
		meanVal[2] += m.vertices[3 * i + 2];
	}
	GLfloat scale = max(maxVal[0] - minVal[0], maxVal[1] - minVal[1]);
	scale = max(scale, maxVal[2] - minVal[2]);

	// Calculate mean values
	for (int i = 0; i < 3; i++)
	{
		meanVal[i] /= (m.obj->numtriangles * 3);
	}

	// Normalization
	for (int i = 0; i < m.obj->numtriangles * 3; i++)
	{
		m.vertices[3 * i + 0] = 1.0*((m.vertices[3 * i + 0] - meanVal[0]) / scale);
		m.vertices[3 * i + 1] = 1.0*((m.vertices[3 * i + 1] - meanVal[1]) / scale);
		m.vertices[3 * i + 2] = 1.0*((m.vertices[3 * i + 2] - meanVal[2]) / scale);
	}
}

void loadOBJModel()
{
	models = new model[filenames.size()];
	int idx = 0;
	for (string filename : filenames)
	{
		models[idx].obj = glmReadOBJ((char*)filename.c_str());
		cout << "Read : " << filename << " OK " << endl;
		traverseColorModel(models[idx++]);
	}
}

void onIdle()
{
	glutPostRedisplay();
}

void drawModel(model* model, int index)
{
	// [TODO] Add an offset before caculate the model position matrix
	// e.x.: offset = index * 2
	// Or you can set the offset value yourself
	GLfloat offset = index * 2;
	Matrix4 T, R, S;
	T = translate(model->position);
	R = rotate(model->rotation);
	S = scaling(model->scale);

	if (start_pos < filenames.size())
	{
		model->position = Vector3(offset, -0.01, 0.0);
		start_pos += 1;
	}

	if (cur_idx!=pre_idx)
	{
		GLfloat distance = (pre_idx - cur_idx) * 2;
		if (anim < filenames.size() * 10)
		{
			model->position += Vector3(distance/10.0, 0.0, 0.0);
			anim += 1;
		}
		if (anim == filenames.size() * 10) {
			pre_idx = cur_idx;
			anim = 0;
		}
	}
	Matrix4 MVP;
	// [TODO] Assign MVP correct value
	// [HINT] MVP = projection_matrix * view_matrix * ??? * ??? * ???
	MVP = project_matrix * view_matrix  * S * T * R;

	// Since the glm matrix is row-major, so we change the matrix to column-major for OpenGL shader.
	// row-major ---> column-major
	GLfloat mvp[16];
	mvp[0] = MVP[0];  mvp[4] = MVP[1];   mvp[8] = MVP[2];    mvp[12] = MVP[3];
	mvp[1] = MVP[4];  mvp[5] = MVP[5];   mvp[9] = MVP[6];    mvp[13] = MVP[7];
	mvp[2] = MVP[8];  mvp[6] = MVP[9];   mvp[10] = MVP[10];   mvp[14] = MVP[11];
	mvp[3] = MVP[12]; mvp[7] = MVP[13];  mvp[11] = MVP[14];   mvp[15] = MVP[15];

	// Link vertex and color matrix to shader paremeter.
	glVertexAttribPointer(iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, model->vertices);
	glVertexAttribPointer(iLocColor, 3, GL_FLOAT, GL_FALSE, 0, model->colors);

	// Drawing command.
	glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, mvp);
	glDrawArrays(GL_TRIANGLES, 0, model->obj->numtriangles * 3);
}

void drawPlane()
{
	GLfloat vertices[18]{ 1.0, -0.6, -1.0,
						   1.0, -0.6,  1.0,
						  -1.0, -0.6, -1.0,
						   1.0, -0.6,  1.0,
						  -1.0, -0.6,  1.0,
						  -1.0, -0.6, -1.0 };

	GLfloat colors[18]{ 0.0,1.0,0.0,
						0.0,0.5,0.8,
						0.0,1.0,0.0,
						0.0,0.5,0.8,
						0.0,0.5,0.8,
						0.0,1.0,0.0 };


	Matrix4 MVP = project_matrix * view_matrix;
	// Since the glm matrix is row-major, so we change the matrix to column-major for OpenGL shader.
	// row-major ---> column-major
	GLfloat mvp[16];
	mvp[0] = MVP[0];  mvp[4] = MVP[1];   mvp[8] = MVP[2];    mvp[12] = MVP[3];
	mvp[1] = MVP[4];  mvp[5] = MVP[5];   mvp[9] = MVP[6];    mvp[13] = MVP[7];
	mvp[2] = MVP[8];  mvp[6] = MVP[9];   mvp[10] = MVP[10];   mvp[14] = MVP[11];
	mvp[3] = MVP[12]; mvp[7] = MVP[13];  mvp[11] = MVP[14];   mvp[15] = MVP[15];

	// [TODO] Draw the plane
	glVertexAttribPointer(iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	glVertexAttribPointer(iLocColor, 3, GL_FLOAT, GL_FALSE, 0, colors);
	// Drawing command.
	glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, mvp);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void onDisplay(void)
{
	// clear canvas
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);	// clear canvas to color(0,0,0)->black
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	drawPlane();

	if (self_rotate)
	{
		// [TODO] Implement self rotation on current model
		models[cur_idx].rotation += Vector3(0.0, -0.005, 0.0);
	}

	for (int i = 0; i < filenames.size(); i++)
	{
		// Draw all models
		modelMovement(&models[i], i);
		drawModel(&models[i], i);
	}
	glutSwapBuffers();
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
	GLuint v, f, p;
	const char **vs = NULL;
	const char **fs = NULL;

	v = glCreateShader(GL_VERTEX_SHADER);
	f = glCreateShader(GL_FRAGMENT_SHADER);

	vs = loadShaderSource("shader.vert");
	fs = loadShaderSource("shader.frag");

	glShaderSource(v, 1, vs, NULL);
	glShaderSource(f, 1, fs, NULL);

	free(vs);
	free(fs);

	// compile vertex shader
	glCompileShader(v);
	GLint vShaderCompiled;
	showShaderCompileStatus(v, &vShaderCompiled);
	if (!vShaderCompiled) system("pause"), exit(123);

	// compile fragment shader
	glCompileShader(f);
	GLint fShaderCompiled;
	showShaderCompileStatus(f, &fShaderCompiled);
	if (!fShaderCompiled) system("pause"), exit(456);

	p = glCreateProgram();

	// bind shader
	glAttachShader(p, f);
	glAttachShader(p, v);

	// link program
	glLinkProgram(p);

	iLocPosition = glGetAttribLocation(p, "av4position");
	iLocColor = glGetAttribLocation(p, "av3color");
	iLocMVP = glGetUniformLocation(p, "mvp");

	glUseProgram(p);

	glEnableVertexAttribArray(iLocPosition);
	glEnableVertexAttribArray(iLocColor);
}

void onMouse(int who, int state, int x, int y)
{
	printf("%18s(): (%d, %d) ", __FUNCTION__, x, y);

	switch (who)
	{
	case GLUT_LEFT_BUTTON:
		current_x = x;
		current_y = y;
		break;
	case GLUT_MIDDLE_BUTTON: printf("middle button "); break;
	case GLUT_RIGHT_BUTTON:
		current_x = x;
		current_y = y;
		break;
	case GLUT_WHEEL_UP:
		printf("wheel up      \n");
		// [TODO] complete all switch case
		switch (cur_trans_mode)
		{
			case ViewEye:
				vieweye += Vector3(0.0, 0.0, 1.0);
				break;
			case ViewCenter:
				viewcenter += Vector3(0.0, 0.0, 1.0);
				break;
			case ViewUp:
				viewup += Vector3(0.0, 0.0, 1.0);
				break;
			case GeoTranslation:
				translation += Vector3(0.0, 0.0, 1.0);
				break;
			case GeoScaling:
				scale += Vector3(0.0, 0.0, 1.0);
				break;
			case GeoRotation:
				rotation += Vector3(0.0, 0.0, 1.0);
				break;
		}
		break;
	case GLUT_WHEEL_DOWN:
		printf("wheel down    \n");
		// [TODO] complete all switch case
		switch (cur_trans_mode)
		{
			case ViewEye:
				vieweye += Vector3(0.0, 0.0, -1.0);
				break;
			case ViewCenter:
				viewcenter += Vector3(0.0, 0.0, -1.0);
				break;
			case ViewUp:
				viewup += Vector3(0.0, 0.0, -1.0);
				break;
			case GeoTranslation:
				translation += Vector3(0.0, 0.0, -1.0);
				break;
			case GeoScaling:
				scale += Vector3(0.0, 0.0, -1.0);
				break;
			case GeoRotation:
				rotation += Vector3(0.0, 0.0, -1.0);
				break;
		}
		break;
	default:
		printf("0x%02X          ", who); break;
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

	// [TODO] complete all switch case
	switch (cur_trans_mode)
	{
		case ViewEye:
			vieweye += Vector3(diff_x, diff_y, 0.0);
			break;
		case ViewCenter:
			viewcenter += Vector3(diff_x, diff_y, 0.0);
			break;
		case ViewUp:
			viewup += Vector3(diff_x, diff_y, 0.0);
			break;
		case GeoTranslation:
			translation = Vector3(diff_x, diff_y, 0.0);
			break;
		case GeoScaling:
			scale = Vector3(diff_x, diff_y, 0.0);
			break;
		case GeoRotation:
			rotation = Vector3(diff_y, diff_x, 0.0);
			break;
	}
	printf("%18s(): (%d, %d) mouse move\n", __FUNCTION__, x, y);
}

void onKeyboard(unsigned char key, int x, int y)
{
	printf("%18s(): (%d, %d) key: %c(0x%02X) ", __FUNCTION__, x, y, key, key);
	string cur_mode = "", cur_mode_p = "";
	switch (key)
	{
	case GLUT_KEY_ESC: /* the Esc key */
		exit(0);
		break;
	case 'z':
		// [TODO] Change model and trigger the model moving animation
		if (anim == 0) {
			pre_idx = cur_idx;
			cur_idx = (cur_idx + filenames.size() - 1) % filenames.size();
		}
		break;
	case 'x':		
		// [TODO] Change model and trigger the model moving animation
		if (anim == 0)
		{
			pre_idx = cur_idx;
			cur_idx = (cur_idx + 1) % filenames.size();
		}
		break;
	case 'w':
		// Change polygon mode
		glPolygonMode(GL_FRONT_AND_BACK, use_wire_mode ? GL_FILL : GL_LINE);
		use_wire_mode = !use_wire_mode;
		break;
	case 'd':
		self_rotate = !self_rotate;
		break;
	case 'o':
		// [TODO] Change to Orthogonal
		proj.left = -1.0;
		proj.right = 1.0;
		proj.top = 1.0;
		proj.bottom = -1.0;
		proj.nearClip = 0.01;
		proj.farClip = 100.0;
		setOrthogonal();
		break;
	case 'p':
		// [TODO] Change to Perspectiver
		proj.left = -1.0;
		proj.right = 1.0;
		proj.top = 1.0;
		proj.bottom = -1.0;
		proj.nearClip = 1.0;
		proj.farClip = 1000.0;
		setPerspective();
		break;
	case 't':
		cur_trans_mode = GeoTranslation;
		break;
	case 's':
		cur_trans_mode = GeoScaling;
		break;
	case 'r':
		cur_trans_mode = GeoRotation;
		break;
	case 'e':
		cur_trans_mode = ViewEye;
		break;
	case 'c':
		cur_trans_mode = ViewCenter;
		break;
	case 'u':
		cur_trans_mode = ViewUp;
		break;
	case 'i':
		// [TODO] print the model name, mode, active operation etc...
		cout << endl << "Model Name : " << filenames[cur_idx] << endl;
		switch (cur_trans_mode)
		{
		case GeoTranslation:
			cur_mode = "GeoTranslation";
			break;
		case GeoScaling:
			cur_mode = "GeoScaling";
			break;
		case  GeoRotation:
			cur_mode = "GeoRotation";
			break;
		case ViewEye:
			cur_mode = "ViewEye";
			break;
		case ViewCenter:
			cur_mode = "ViewCenter";
			break;
		case ViewUp:
			cur_mode = "ViewUp";
			break;
		default:
			cur_mode = "other";
			break;
		}
		cur_mode_p = (cur_proj_mode == Perspective) ? "PERSPECTIVE" : "ORTHOGONAL";
		cout << endl << "current ProjMode : " << cur_mode_p << endl;
		cout << "project setting : [left, right, top, bottom, nearClip, farClip, fov] = [";
		cout << proj.left << ", " << proj.right << ", " << proj.top << ", " << proj.bottom << ", " << proj.nearClip << ", " << proj.farClip << ", " << proj.fovy << "]" << endl;
		cout << "current TransMode : " << cur_mode << endl;
		cout << "position of the model : " << models[cur_idx].position << endl;
		cout << "scaling of the model : " << models[cur_idx].scale << endl;
		cout << "rotation of the model : " << models[cur_idx].rotation << endl;
		cout << "position of the camera : " << main_camera.position << endl;
		cout << "center of the camera : " << main_camera.center << endl;
		break;
	}
	printf("\n");
}

void onKeyboardSpecial(int key, int x, int y) {
	printf("%18s(): (%d, %d) ", __FUNCTION__, x, y);
	switch (key)
	{
	case GLUT_KEY_LEFT:
		printf("key: LEFT ARROW");
		break;

	case GLUT_KEY_RIGHT:
		printf("key: RIGHT ARROW");
		break;

	default:
		printf("key: 0x%02X      ", key);
		break;
	}
	printf("\n");
}

void onWindowReshape(int width, int height)
{
	proj.aspect = width / height;

	printf("%18s(): %dx%d\n", __FUNCTION__, width, height);
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

// you can setup your own camera setting when testing
void initParameter()
{
	proj.left = -1.0;
	proj.right = 1.0;
	proj.top = 1.0;
	proj.bottom = -1.0;
	proj.nearClip = 1.0;
	proj.farClip = 1000.0;

	main_camera.position = Vector3(0.0f, 0.0f, 0.0f);
	main_camera.center = Vector3(0.0f, 0.0f, 2.0f);
	main_camera.up_vector = Vector3(0.0f, -1.0f, 0.0f);

	setViewingMatrix();
	setPerspective();	//set default projection matrix as orthogonal matrix
}

int main(int argc, char **argv)
{
	loadConfigFile();

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);

	// create window
	glutInitWindowPosition(500, 100);
	glutInitWindowSize(800, 800);
	glutCreateWindow("CG HW2");

	glewInit();
	if (glewIsSupported("GL_VERSION_2_0")) {
		printf("Ready for OpenGL 2.0\n");
	}
	else {
		printf("OpenGL 2.0 not supported\n");
		system("pause");
		exit(1);
	}

	// initialize the camera parameter
	initParameter();

	// load obj models through glm
	loadOBJModel();

	// register glut callback functions
	glutDisplayFunc(onDisplay);
	glutIdleFunc(onIdle);
	glutKeyboardFunc(onKeyboard);
	glutSpecialFunc(onKeyboardSpecial);
	glutMouseFunc(onMouse);
	glutMotionFunc(onMouseMotion);
	glutReshapeFunc(onWindowReshape);

	// set up shaders here
	setShaders();

	glEnable(GL_DEPTH_TEST);

	// main loop
	glutMainLoop();

	// delete glm objects before exit
	for (int i = 0; i < filenames.size(); i++)
	{
		glmDelete(models[i].obj);
	}

	return 0;
}

