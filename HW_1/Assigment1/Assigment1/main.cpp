#include <stdlib.h>
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

	Vector3 position = Vector3(0,0,0);
	Vector3 scale = Vector3(1,1,1);
	Vector3 rotation = Vector3(0,0,0);	// Euler form
};

struct project_setting
{
	GLfloat nearClip, farClip;
	GLfloat fovy;
	GLfloat aspect;
	GLfloat left, right, top, bottom;
};

Matrix4 view_matrix;
Matrix4 project_matrix;

project_setting proj;
camera main_camera;

int current_x, current_y;

model* models;	// store the models we load
vector<string> filenames; // .obj filename list
int cur_idx = 0; // represent which model should be rendered now
bool use_wire_mode = false;

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
	Matrix4 mat;
	
	mat = Matrix4(
	vec.x, 0.0f, 0.0f, 0.0f,
	0.0f, vec.y, 0.0f, 0.0f,
	0.0f, 0.0f, vec.z, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
	);
	return mat;
}

Matrix4 rotateX(GLfloat val)
{
	Matrix4 mat;
	
	
	mat = Matrix4(
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, cos(val), -sin(val), 0.0f,
	0.0f, sin(val), cos(val), 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
	);
	return mat;
}

Matrix4 rotateY(GLfloat val)
{
	Matrix4 mat;
	
	mat = Matrix4(
	cos(val), 0.0f, sin(val), 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	-sin(val), 0.0f, cos(val), 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
	);
	return mat;
}

Matrix4 rotateZ(GLfloat val)
{
	Matrix4 mat;
	
	mat = Matrix4(
	cos(val), -sin(val), 0.0f, 0.0f,
	sin(val), cos(val), 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
	);
	return mat;
}

Matrix4 rotate(Vector3 vec)
{
	return rotateX(models[cur_idx].rotation.x + vec.x)*rotateY(models[cur_idx].rotation.y + vec.y)*rotateZ(models[cur_idx].rotation.z + vec.z);
}


void setViewingMatrix()
{
	Vector3 VE = Vector3(main_camera.position.x, main_camera.position.y, main_camera.position.z + 2.0f);
	Vector3 VC = Vector3(main_camera.center.x, main_camera.center.y, main_camera.center.z - 5.0f);
	Vector3 VU = main_camera.up_vector;
	Vector3 forward = VC - VE;
	Vector3 right = forward.cross(VU);
	Vector3 up = right.cross(forward);
	Vector3 RX = forward.cross(VU) / (forward.cross(VU)).length();
	Vector3 RZ = forward / forward.length();
	Vector3 RY = RX.cross(RZ);

	Matrix4 VR;
	VR.setRow(0, RX);
	VR.setRow(1, RY);
	VR.setRow(2, -RZ);
	VR.setRow(3, Vector4(0.0f, 0.0f, 0.0f, 1.0f));

	Matrix4 VT = Matrix4(
		1.0f, 0.0f, 0.0f, -VE.x,
		0.0f, 1.0f, 0.0f, -VE.y,
		0.0f, 0.0f, 1.0f, -VE.z,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	view_matrix = VR * VT;
}


void setPerspective()
{
	project_matrix = Matrix4(
		2 * proj.nearClip / (proj.right - proj.left), 0.0f, (proj.right + proj.left) / (proj.right - proj.left), 0.0f,
		0.0f, 2 * proj.nearClip / (proj.top - proj.bottom), (proj.top + proj.bottom) / (proj.top - proj.bottom), 0.0f,
		0.0f, 0.0f, -1 * (proj.farClip + proj.nearClip) / (proj.farClip - proj.nearClip), -2 * proj.farClip * proj.nearClip / (proj.farClip - proj.nearClip),
		0.0f, 0.0f, -1.0f, 0.0f
	);
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

	//printf("#triangles: %d\n", m.obj->numtriangles);


	for (int i = 0; i < (int)m.obj->numtriangles; i++)
	{
		//[TODO] Put structure value(m.obj->vertices,m.obj->colors)	to 1D array (m.vertices and m.colors).

		// the index of each vertex
		int indexv1 = m.obj->triangles[i].vindices[0];
		int indexv2 = m.obj->triangles[i].vindices[1];
		int indexv3 = m.obj->triangles[i].vindices[2];

		// assign vertices
		GLfloat v1, v2, v3;
		v1 = m.obj->vertices[indexv1 * 3 + 0];
		v2 = m.obj->vertices[indexv1 * 3 + 1];
		v3 = m.obj->vertices[indexv1 * 3 + 2];

		m.vertices[i * 9 + 0] = v1;
		m.vertices[i * 9 + 1] = v2;
		m.vertices[i * 9 + 2] = v3;

		v1 = m.obj->vertices[indexv2 * 3 + 0];
		v2 = m.obj->vertices[indexv2 * 3 + 1];
		v3 = m.obj->vertices[indexv2 * 3 + 2];

		m.vertices[i * 9 + 3] = v1;
		m.vertices[i * 9 + 4] = v2;
		m.vertices[i * 9 + 5] = v3;

		v1 = m.obj->vertices[indexv3 * 3 + 0];
		v2 = m.obj->vertices[indexv3 * 3 + 1];
		v3 = m.obj->vertices[indexv3 * 3 + 2];

		m.vertices[i * 9 + 6] = v1;
		m.vertices[i * 9 + 7] = v2;
		m.vertices[i * 9 + 8] = v3;

		// assign colors
		GLfloat c1, c2, c3;
		c1 = m.obj->colors[indexv1 * 3 + 0];
		c2 = m.obj->colors[indexv1 * 3 + 1];
		c3 = m.obj->colors[indexv1 * 3 + 2];

		m.colors[i * 9 + 0] = c1;
		m.colors[i * 9 + 1] = c2;
		m.colors[i * 9 + 2] = c3;

		c1 = m.obj->colors[indexv2 * 3 + 0];
		c2 = m.obj->colors[indexv2 * 3 + 1];
		c3 = m.obj->colors[indexv2 * 3 + 2];

		m.colors[i * 9 + 3] = c1;
		m.colors[i * 9 + 4] = c2;
		m.colors[i * 9 + 5] = c3;

		c1 = m.obj->colors[indexv3 * 3 + 0];
		c2 = m.obj->colors[indexv3 * 3 + 1];
		c3 = m.obj->colors[indexv3 * 3 + 2];

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
		//meanVal[i] = (maxVal[i] + minVal[i]) / 2.0;
		meanVal[i] /= (m.obj->numtriangles*3);
	}

	// Normalization
	for (int i = 0; i < m.obj->numtriangles * 3; i++)
	{
		//[TODO] Normalize each vertex value.
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
		traverseColorModel(models[idx++]);
	}
}

void onIdle()
{
	glutPostRedisplay();
}

void drawModel(model* model)
{
	Matrix4 T, R, S;
	T = translate(model->position);
	R = rotate(model->rotation);
	S = scaling(model->scale);

	// [TODO] Assign MVP correct value
	// [HINT] MVP = projection_matrix * view_matrix * ??? * ??? * ???

	setViewingMatrix();
	Matrix4 MVP = project_matrix * view_matrix * T * S * R;
	GLfloat mvp[16];

	// row-major ---> column-major
	
	mvp[0] = MVP[0];  mvp[4] = MVP[1];   mvp[8] = MVP[2];    mvp[12] = MVP[3];
	mvp[1] = MVP[4];  mvp[5] = MVP[5];   mvp[9] = MVP[6];    mvp[13] = MVP[7];
	mvp[2] = MVP[8];  mvp[6] = MVP[9];   mvp[10] = MVP[10];   mvp[14] = MVP[11];
	mvp[3] = MVP[12]; mvp[7] = MVP[13];  mvp[11] = MVP[14];   mvp[15] = MVP[15];
	
	glVertexAttribPointer(iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, model->vertices);
	glVertexAttribPointer(iLocColor, 3, GL_FLOAT, GL_FALSE, 0, model->colors);

	// assign mvp value to shader's uniform variable at location iLocMVP
	glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, mvp);
	glDrawArrays(GL_TRIANGLES, 0, model->obj->numtriangles * 3);
}


void onDisplay(void)
{
	// clear canvas
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);	// clear canvas to color(0,0,0)->black
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	drawModel(&models[cur_idx]);

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
			break;
		case GLUT_WHEEL_DOWN:
			printf("wheel down    \n");
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
	case 'c':
		use_wire_mode = !use_wire_mode;
		if (use_wire_mode)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		else
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
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


void initParameter()
{
	proj.left = -1;
	proj.right = 1;
	proj.top = 1;
	proj.bottom = -1;
	proj.nearClip = 1.0;
	proj.farClip = 5.0;
	proj.fovy = 60;

	main_camera.position = Vector3(0.0f, 0.0f, 0.0f);
	main_camera.center = Vector3(0.0f, 0.0f, 0.0f);
	main_camera.up_vector = Vector3(0.0f, 1.0f, 0.0f);

	setViewingMatrix();
	setPerspective();
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

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);

	// create window
	glutInitWindowPosition(500, 100);
	glutInitWindowSize(800, 800);
	glutCreateWindow("CG HW1");

	glewInit();
	if (glewIsSupported("GL_VERSION_2_0")) {
		printf("Ready for OpenGL 2.0\n");
	}
	else {
		printf("OpenGL 2.0 not supported\n");
		system("pause");
		exit(1);
	}

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

