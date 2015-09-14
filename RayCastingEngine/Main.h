#include <stdio.h>
#include <stdlib.h>
#include <tuple>
#include <deque>
#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <map>
#include <set>
#include <math.h>
#include <Windows.h>
#include <gl\glut.h>

typedef std::vector<float> floatvec;
typedef std::vector<floatvec> floatmat;
typedef std::vector<double> doublevec;
typedef std::vector<doublevec> doublemat;
typedef std::vector<int> intvec;
typedef std::vector<char*> stringvec;
typedef std::pair<float, float> floatpair;
typedef std::map<floatpair, floatvec> levellayout;
typedef std::vector<bool> boolvec;
typedef std::vector<boolvec> boolmat;

#ifndef GL_BGR
#define GL_BGR GL_BGR_EXT
#endif

const double PI = 3.14159265358979323846;

// Changeable parameters
float posX = 3.0f;
float posY = 0.0f;
float posZ = 3.0f;
float angleX = 0.0f;
float angleY = 0.0f;
const float playerHeight = 2.115275;
const float crouchSpeed = 0.2;
float walkingSpeed = 0.3f;
float crouchingSpeed = 0.1f;
int screenW = 720;
int screenH = 540;
const float fMin = 0.5;
const float fMax = 100.0;
float alpha = 60.0;
float jumpVelocity = 0.305;
float gravity = -0.0125;
float smoothX = 0.2f;
const float stepThreshold = 0.51;
const float gridSep = 4.25;
const float viewRange = 25.0;
const int prevListCount = 200;
const int prevListStep = 2;
const int fontSize = 12;
const int fontSep = 5;
void * mainFont = GLUT_BITMAP_HELVETICA_12;
const float noCeilingHeight = 100000.0;
const float epsilon = 0.0001;

float Mod(float a, float b);

struct Vector3 {
	doublevec Vec;

	Vector3() {
		Vec = doublevec(3);
	}

	Vector3(double X, double Y, double Z) {
		Vec = doublevec(3);
		Vec[0] = X;
		Vec[1] = Y;
		Vec[2] = Z;
	}

	double& operator()(unsigned int i) {
		return Vec[i];
	}

	Vector3 * ToPixels(double w, double h) {
		return new Vector3(((Vec[0] + 1) / 2.0) * w, Vec[1], ((-Vec[2] + 1) / 2.0) * h);
	}

	Vector3 * Minus(Vector3 * other) {
		return new Vector3(Vec[0] - other->operator()(0), Vec[1] - other->operator()(1), Vec[2] - other->operator()(2));
	}

	Vector3 * Plus(Vector3 * other) {
		return new Vector3(Vec[0] + other->operator()(0), Vec[1] + other->operator()(1), Vec[2] + other->operator()(2));
	}
};

typedef std::deque<Vector3*> posdeq;

struct LevelMap {
	levellayout layout; // The layout of this section
	float maxX, maxY, maxZ; // The size of the layout
	int gameSection; // To which gamesection this map belongs
	bool connected = false; // If this levelmap has already added a connection before, used to prevent cycles

	LevelMap() {}

	LevelMap(char* fileName, int gameSection) {
		ReadMap(fileName);
		this->gameSection = gameSection;
	}

	// Opens a text file and assigns levelmap1, then runs Rectanglify for floorrect and ceilingrect
	void ReadMap(char * fileName) {
		layout = levellayout();
		std::fstream file(fileName, std::ios_base::in);

		file >> maxX >> maxY >> maxZ;

		float a1, a2, a3, a4;
		floatpair p;
		while (file >> a1 >> a2 >> a3 >> a4) {
			p.first = a1 - 1;
			p.second = a2 - 1;
			if (a4 == -1)
				a4 = noCeilingHeight;
			layout[p] = { a3, a4 };
		}

		file.close();
	}

	// Requirements for a valid connection:
	// - The block next to exit1 in direction dir1 is empty
	// - The block next to exit2 in direction dir2 is empty
	void AddConnection(Vector3 exit1, int dir1, LevelMap* level2, Vector3 exit2, int dir2) {
		if (level2->connected)
			return;

		// Determines rotation and offset
		int rot = Mod(dir1 - dir2 - 180, 360); // counter-clockwise, otherwise: dir2 - dir1

		// Rotates (counter-clockwise) the layout of level2
		floatpair relLoc = level2->RotateLayout(rot, exit2); // The (new) relative location of the exit
		int xAdd = 0, zAdd = 0;
		if (dir1 == 0)
			xAdd = 1;
		if (dir1 == 90)
			zAdd = -1;
		if (dir1 == 180)
			xAdd = -1;
		if (dir1 == 270)
			zAdd = 1;
		floatpair absLoc = floatpair(exit1(0) + xAdd, exit1(2) + zAdd); // The actual absolute location of the exit

		// Offsets the layout of level2 s.t. the exit is in it's absolute location
		level2->OffsetLayout(absLoc.first - relLoc.first, exit1(1) - exit2(1), absLoc.second - relLoc.second);

		if (!connected)
			connected = true;
	}

	// Rotates layout and returns the new coordinates of exit
	floatpair RotateLayout(int rot, Vector3 exit) {
		float tempX, tempZ;
		levellayout templayout;
		floatpair exitpair;
		if (rot == 90) { // Rotate 90 degrees counter-clockwise
			// Swaps maxX and maxZ
			std::swap(maxX, maxZ);
			for (levellayout::iterator it = layout.begin(); it != layout.end(); ++it) {
				tempX = it->first.first;
				tempZ = it->first.second;
				// Transpose
				std::swap(tempX, tempZ);
				// Reverse column
				tempZ = maxZ - tempZ - 1;
				templayout[floatpair(tempX, tempZ)] = it->second;
				if (tempX == exit(0) && tempZ == exit(2))
					exitpair = it->first;
			}
		}
		if (rot == 180) { // Rotate 180 degrees
			for (levellayout::iterator it = layout.begin(); it != layout.end(); ++it) {
				tempX = it->first.first;
				tempZ = it->first.second;
				// Reverse row
				tempX = maxX - tempX - 1;
				// Reverse column
				tempZ = maxZ - tempZ - 1;
				templayout[floatpair(tempX, tempZ)] = it->second;
				if (tempX == exit(0) && tempZ == exit(2))
					exitpair = it->first;
			}
		}
		if (rot == 270) { // Rotate 90 degrees clockwise
			// Swaps maxX and maxZ
			std::swap(maxX, maxZ);
			for (levellayout::iterator it = layout.begin(); it != layout.end(); ++it) {
				tempX = it->first.first;
				tempZ = it->first.second;
				// Transpose
				std::swap(tempX, tempZ);
				// Reverse row
				tempX = maxX - tempX - 1;
				templayout[floatpair(tempX, tempZ)] = it->second;
				if (tempX == exit(0) && tempZ == exit(2))
					exitpair = it->first;
			}
		}
		layout = templayout;
		return exitpair;
	}

	// Offsets the values in layout
	void OffsetLayout(float xOff, float yOff, float zOff) {
		float tempX, tempZ;
		levellayout templayout;
		for (levellayout::iterator it = layout.begin(); it != layout.end(); ++it) {
			tempX = it->first.first + xOff;
			tempZ = it->first.second + zOff;
			it->second[0] = it->second[0] + yOff;
			it->second[1] = it->second[1] + yOff;
			templayout[floatpair(tempX, tempZ)] = it->second;
		}
		layout = templayout;
	}
};

// Loads a .bmp file as texture, returns texture data in the form of a GLuint
// TODO: doesn't work yet
//GLuint LoadTexture(const char * fileName) {
//	// Data read from the header of the BMP file
//	unsigned char header[54]; // Each BMP file begins by a 54-bytes header
//	unsigned int dataPos;     // Position in the file where the actual data begins
//	unsigned int width, height;
//	unsigned int imageSize;   // = width*height*3
//							  // Actual RGB data
//	unsigned char * data;
//
//	// Open the file
//	FILE * file = fopen(fileName, "rb");
//	if (!file) {
//		printf("Image could not be opened\n");
//		return 0;
//	}
//
//	if (fread(header, 1, 54, file) != 54) { // If not 54 bytes read : problem
//		printf("Not a correct BMP file\n");
//		return false;
//	}
//
//	if (header[0] != 'B' || header[1] != 'M') {
//		printf("Not a correct BMP file\n");
//		return 0;
//	}
//
//	// Read ints from the byte array
//	dataPos = *(int*)&(header[0x0A]);
//	imageSize = *(int*)&(header[0x22]);
//	width = *(int*)&(header[0x12]);
//	height = *(int*)&(header[0x16]);
//
//	// Some BMP files are misformatted, guess missing information
//	if (imageSize == 0)    imageSize = width*height * 3; // 3 : one byte for each Red, Green and Blue component
//	if (dataPos == 0)      dataPos = 54; // The BMP header is done that way
//
//	// Create a buffer
//	data = new unsigned char[imageSize];
//
//	// Read the actual data from the file into the buffer
//	fread(data, 1, imageSize, file);
//
//	//Everything is in memory now, the file can be closed
//	fclose(file);
//
//	// Create one OpenGL texture
//	GLuint textureID;
//	glGenTextures(1, &textureID);
//
//	// "Bind" the newly created texture : all future texture functions will modify this texture
//	glBindTexture(GL_TEXTURE_2D, textureID);
//
//	// Give the image to OpenGL
//	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data);
//
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//}