/*
Goal of this project:

1. Make a raycasting engine for a camera that can have any position, height, horizontal and vertical view angle, horizontal and vertical view size and total view angle. Tilting the view angle will be excluded
2. The scenes may consist of any 3d convex polygon or 2d convex plane, where an bitmap is projected onto the plane and sides of the polygon
3. Use Lua to make scripted cutscenes (by e.g. moving the camera)
4. Use OpenGL for the graphics related problems
*/

#include "Main.h"

// Variable initialization
bool* keyTable = new bool[256];
bool* keyHandled = new bool[256];
float speed;
std::pair<int, int> direction;
LevelMap* currentMap;
std::vector<LevelMap*> activeMaps;
float velocityX = 0.0;
float velocityY = 0.0;
float velocityZ = 0.0;
bool flying = false;
bool jumping = false;
bool inAir = false;
bool crouching = false;
bool showRedLine = false;
float inAirDuration, diffY;
float smoothY = smoothX * (float)screenW / (float)screenH;
float newX, newZ;
float groundHeight = 0.0;
float fps = 0.0;
int frames2 = 0.0;
posdeq prevPosList;
float nH, lH, rH, uH, dH;
float nC, lC, rC, uC, dC;
bool collisionCheck;
double tanAlpha = fMin * tan((alpha / 2.0) * PI / 180.0);
double res = (float)screenW / (float)screenH;
double minDist = fMin / (cos((alpha / 2.0) * PI / 180.0) * cos(((alpha * res) / 2.0) * PI / 180.0));
int centerX = screenW / 2;
int centerY = screenH / 2;
const float crouchHeight = stepThreshold + minDist;
GLuint texture;

////////// Elementary functions

// Returns a mod b
float Mod(float a, float b) {
	return a - b * floor(a / b);
}

// Converts a bool to a char
char * BoolToChar(bool b) {
	return b ? "True" : "False";
}

// Converts pixel coordinates to normalized coordinates
floatpair PixToNorm(int x, int y) {
	return floatpair(2.0 * (float)x / (float)screenW - 1, 1 - 2.0 * (float)y / (float)screenH);
}

// Writes the string "a = b" to the char str
void DisplayVar(char * a, bool b, char * str) {
	strcpy(str, a);
	strcat(str, BoolToChar(b));
}

////////// Additional functions

// Gets the ground height for the current posX and posZ values
float GetGroundHeight(float x, float z, LevelMap * levelmap) {
	floatpair f = floatpair((int)floor(x / gridSep), (int)floor(z / gridSep));
	if (levelmap->layout.find(f) != levelmap->layout.end()) {
		return levelmap->layout[f][0];
	} else {
		return noCeilingHeight;
	}
}

// Gets the ceiling height for the current posX and posZ values
float GetCeilingHeight(float x, float z, LevelMap * levelmap) {
	floatpair f = floatpair((int)floor(x / gridSep), (int)floor(z / gridSep));
	if (levelmap->layout.find(f) != levelmap->layout.end()) {
		return levelmap->layout[f][1];
	} else {
		return noCeilingHeight;
	}
}

// Retrieves the current map from the list of active maps
// TODO: change this
LevelMap * GetCurrentMap(float x, float z) {
	return activeMaps[0];
}

////////// Draw functions

// Draws a polygon with 4 sides and gives it a texture
// TODO: doesn't work yet with textures
void DrawQuad(floatvec c) {
	// Draw Quad and give it the texture using a 'vertex shader'
	glPushAttrib(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glTexCoord2d(0, 0);
	glVertex3f(-c[0], -c[1], -c[2]);
	glTexCoord2d(0, 1);
	glVertex3f(-c[3], -c[4], -c[5]);
	glTexCoord2d(1, 1);
	glVertex3f(-c[6], -c[7], -c[8]);
	glTexCoord2d(1, 0);
	glVertex3f(-c[9], -c[10], -c[11]);
	glEnd();
	glDisable(GL_TEXTURE_2D);
	glPopAttrib();
}

// Draws a line in 3D given the coordinates
void DrawLine3(float x1, float y1, float z1, float x2, float y2, float z2) {
	glBegin(GL_LINES);
	glVertex3f((GLfloat)-x1, (GLfloat)-y1, (GLfloat)-z1);
	glVertex3f((GLfloat)-x2, (GLfloat)-y2, (GLfloat)-z2);
	glEnd();
}

// Draws a line in 2D given the normalized coordinates
void DrawLine2(float x1, float y1, float x2, float y2) {
	glBegin(GL_LINES);
	glVertex2f((GLfloat)x1 * tanAlpha * res, (GLfloat)y1 * tanAlpha);
	glVertex2f((GLfloat)x2 * tanAlpha * res, (GLfloat)y2 * tanAlpha);
	glEnd();
}

// Draws a line in 2D given the pixels of both points
void DrawLine2Pix(int x1, int y1, int x2, int y2) {
	floatpair p1 = PixToNorm(x1, y1);
	floatpair p2 = PixToNorm(x2, y2);
	DrawLine2(p1.first, p1.second, p2.first, p2.second);
}

// Draws text on screen given the pixel location of the top left corner
void DrawText2Pix(char * text, int x, int y) {
	floatpair p = PixToNorm(x, y + fontSize);
	glRasterPos2f(p.first * tanAlpha * res, p.second * tanAlpha);
	for (char * c = text; *c != '\000'; c++) {
		glutBitmapCharacter(mainFont, *c);
	}
}

// Draws multple lines of text on screen given the pixel location of the top left corner
void DrawText2Pix(stringvec lines, int x, int y) {
	int add = 0;
	for (stringvec::iterator it = lines.begin(); it != lines.end(); ++it) {
		DrawText2Pix(*it, x, y + add);
		add += fontSize + fontSep;
	}
}

// Draws a deque of Vector3's
void DrawPosDeq(posdeq vec) {
	posdeq::iterator it = vec.begin();
	while (it + 1 != vec.end()) {
		auto it2 = it;
		it2++;
		DrawLine3((*it)->Vec[0], (*it)->Vec[1], (*it)->Vec[2], (*it2)->Vec[0], (*it2)->Vec[1], (*it2)->Vec[2]);
		it = it2;
	}
}

////////// Event functions

// Adjusts the players position and velocity given a direction and a distance
void Move(double dist, double dir) {
	double dirFly = (dir > 90.0 && dir < 270.0) ? 180.0 : 0.0;
	int move = (direction.first == 0 && direction.second == 0) ? 0 : 1;
	if (!flying) {
		velocityX = move * dist * sin((angleX + dir) * PI / 180.0);
		velocityZ = move * dist * cos((angleX + dir) * PI / 180.0);
		newX = posX - velocityX;
		newZ = posZ - velocityZ;
	} else {
		velocityZ = move * dist * ((direction.first != 0 && direction.second == 0) ? -1 : cos((angleY - 180) * PI / 180.0)) * cos((angleX + dir) * PI / 180.0);
		velocityX = move * dist * ((direction.first != 0 && direction.second == 0) ? -1 : cos((angleY - 180) * PI / 180.0)) * sin((angleX + dir) * PI / 180.0);
		velocityY = move * abs(direction.second) * dist * sin((angleY + dirFly) * PI / 180.0);
		posX += velocityX;
		posY += velocityY;
		posZ += velocityZ;
	}
}

// Checks whether the player is falling down, and if so, adjusts the player's Y position and velocity
void FallingCheck() {
	if (!inAir) {
		if (posY + velocityY > groundHeight + playerHeight) {
			inAir = true;
			inAirDuration = 0;
		}
	}
	if (inAir) {
		inAirDuration += 1.0;
		diffY = velocityY + gravity * inAirDuration;
		if (posY + diffY <= groundHeight + playerHeight) {
			posY = groundHeight + playerHeight;
			velocityY = 0;
			inAir = false;
			jumping = false;
		} else {
			posY += diffY;
		}
	}
}

// Checks inputs and moves the player. Gets called each frame
void CheckInputs() {
	// Checks inputs
	if (keyTable['d'])
		direction.first = -1;
	if (keyTable['a'])
		direction.first = 1;
	if (!keyTable['d'] == !keyTable['a'])
		direction.first = 0;
	if (keyTable['s'])
		direction.second = -1;
	if (keyTable['w'])
		direction.second = 1;
	if (!keyTable['s'] == !keyTable['w'])
		direction.second = 0;
	if ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) && !inAir && !flying) { // Ctrl
		posY = max(posY - crouchSpeed, groundHeight + crouchHeight);
		if (!crouching)
			crouching = true;
	}
	if (!(GetAsyncKeyState(VK_LCONTROL) & 0x8000) && crouching && posY != groundHeight + playerHeight && !flying && !inAir) {
		posY = min(posY + crouchSpeed, groundHeight + playerHeight);
		if (crouching && posY == groundHeight + playerHeight)
			crouching = false;
	}
	if (keyTable[32] && !keyHandled[32] && !inAir && !crouching && !flying) { // Space
		velocityY = jumpVelocity;
		jumping = true;
		keyHandled[32] = true;
	}
	if (keyTable['f'] && !keyHandled['f'] && !crouching) {
		flying = !flying;
		keyHandled['f'] = true;
		inAirDuration = 0;
	}
	if (keyTable['r'] && !keyHandled['r'] && !crouching) {
		showRedLine = !showRedLine;
		keyHandled['r'] = true;
	}
	if (keyTable[27])  // Escape
		exit(0);

	// Finds the X and Z coordinates of the player after it has moved in the direction that it is going
	if (crouching)
		speed = crouchingSpeed;
	if (!crouching || flying)
		speed = walkingSpeed;
	Move(speed, 180.0 - 90.0 * direction.first + 45.0 * direction.first * direction.second - 180.0 * (((direction.second == -1 || direction.second == 0) && direction.first == 0) ? 1 : 0));

	// TODO: add wall strafing
	if (!flying) {
		// Checks for collisions with walls in up, left, right and down directions
		nH = GetGroundHeight(newX, newZ, currentMap);
		lH = GetGroundHeight(newX - minDist, newZ, currentMap);
		rH = GetGroundHeight(newX + minDist, newZ, currentMap);
		uH = GetGroundHeight(newX, newZ - minDist, currentMap);
		dH = GetGroundHeight(newX, newZ + minDist, currentMap);
		collisionCheck = true;
		if ((lH + playerHeight - posY > stepThreshold && !crouching) || (lH + crouchHeight - posY > stepThreshold && crouching)) {
			posX = floor(posX / gridSep) * gridSep + minDist + epsilon;
			collisionCheck = false;
		}
		if ((rH + playerHeight - posY > stepThreshold && !crouching) || (rH + crouchHeight - posY > stepThreshold && crouching)) {
			posX = ceil(posX / gridSep) * gridSep - minDist - epsilon;
			collisionCheck = false;
		}
		if ((uH + playerHeight - posY > stepThreshold && !crouching) || (uH + crouchHeight - posY > stepThreshold && crouching)) {
			posZ = floor(posZ / gridSep) * gridSep + minDist + epsilon;
			collisionCheck = false;
		}
		if ((dH + playerHeight - posY > stepThreshold && !crouching) || (dH + crouchHeight - posY > stepThreshold && crouching)) {
			posZ = ceil(posZ / gridSep) * gridSep - minDist - epsilon;
			collisionCheck = false;
		}

		// Checks for step-ups
		if (((nH + playerHeight - posY <= stepThreshold && !crouching) || (nH + crouchHeight - posY <= stepThreshold && crouching)))
			groundHeight = nH;

		// Sets posY if it is below the correct position
		if (posY < groundHeight + playerHeight && !crouching)
			posY = groundHeight + playerHeight;
		if (posY < groundHeight + crouchHeight && crouching)
			posY = groundHeight + crouchHeight;

		// Sets posY if it is above the ceiling
		nC = GetCeilingHeight(newX, newZ, currentMap);
		if (posY > nC - minDist - epsilon) {
 			posY = nC - minDist - epsilon;
			velocityY = 0.0;
		}

		// Checks for collisions between ceilings of different heights
		lC = GetCeilingHeight(newX - minDist, newZ, currentMap);
		rC = GetCeilingHeight(newX + minDist, newZ, currentMap);
		uC = GetCeilingHeight(newX, newZ - minDist, currentMap);
		dC = GetCeilingHeight(newX, newZ + minDist, currentMap);
		if (posY + minDist > lC && lC != noCeilingHeight) {
			posX = floor(posX / gridSep) * gridSep + minDist;
			collisionCheck = false;
		}
		if (posY + minDist > rC && rC != noCeilingHeight) {
			posX = ceil(posX / gridSep) * gridSep - minDist;
			collisionCheck = false;
		}
		if (posY + minDist > uC && uC != noCeilingHeight) {
			posZ = floor(posZ / gridSep) * gridSep + minDist;
			collisionCheck = false;
		}
		if (posY + minDist > dC && dC != noCeilingHeight) {
			posZ = ceil(posZ / gridSep) * gridSep - minDist;
			collisionCheck = false;
		}

		// If no collision: move to new position
		if (collisionCheck) {
			posX = newX;
			posZ = newZ;
		}

		// Checks whether the player is falling down
		FallingCheck();
	}
}

// Changes the angle of the camera based on mouse movement, then re-centers the mouse cursor
void MouseMovement(int x, int y) {
	angleX = Mod(angleX + (float)(x - centerX) * smoothX, 360.0);
	angleY = max(min(angleY + (float)(y - centerY) * smoothY, 90.0F), -90.0F);
	if (x != centerX || y != centerY)
		glutWarpPointer(centerX, centerY);
}

// Removes the Ctrl modifier from the key and returns the key without the modifier
void RemoveCtrlModifier(unsigned char * key) {
	if (*key == '\027') // Ctrl + W
		*key = 'w';
	if (*key == '\001') // Ctrl + A
		*key = 'a';
	if (*key == '\023') // Ctrl + S
		*key = 's';
	if (*key == '\004') // Ctrl + D
		*key = 'd';
}

// Registers a key as being pressed down
void KeyPressed(unsigned char key, int x, int y) {
	RemoveCtrlModifier(&key);
	if (!keyTable[key]) {
		keyTable[key] = true;
		//std::cout << "Key Pressed: " << key << " (" << static_cast<unsigned>(key) << ")" << std::endl;
	}
}

// Registers a key as being released
void KeyUp(unsigned char key, int x, int y) {
	RemoveCtrlModifier(&key);
	if (keyTable[key])
		keyTable[key] = false;
	if (keyHandled[key])
		keyHandled[key] = false;
}

// Displays fps after a second has passed
void SecondPassed(int value) {
	std::cout << "fps = " << fps << std::endl;
	fps = 0.0;
	glutTimerFunc(1000, SecondPassed, 0);
}

// Draws crosshairs
void DrawCrosshairs(int l, int sep) {
	DrawLine2Pix(centerX - (l + sep + 1), centerY, centerX - (sep + 1), centerY); // Left
	DrawLine2Pix(centerX + (l + sep), centerY, centerX + sep, centerY); // Right
	DrawLine2Pix(centerX, centerY - (l + sep), centerX, centerY - sep); // Up
	DrawLine2Pix(centerX, centerY + l + sep + 1, centerX, centerY + sep + 1); // Down
}

// Draws a grid on the floor
void DrawGrid() {
	glColor3f(1.0, 1.0, 1.0); // White
	for (float x = -(viewRange * gridSep); x <= viewRange * gridSep; x = x + gridSep) {
		DrawLine3(x, 0, viewRange * gridSep, x, 0, -(viewRange * gridSep)); // Vertical lines
		DrawLine3(viewRange * gridSep, 0, x, -(viewRange * gridSep), 0, x); // Horizontal lines
	}
}

// Draws the given map
void DrawMap(levellayout levelmap) {
	float i, j;
	floatpair f;
	for (levellayout::iterator it = levelmap.begin(); it != levelmap.end(); ++it) {
		i = it->first.first;
		j = it->first.second;

		glColor3f(1.0, 1.0, 1.0); // White
		DrawQuad({ gridSep * i, it->second[1], gridSep * j,
			gridSep * (i + 1), it->second[1], gridSep * j,
			gridSep * (i + 1), it->second[1], gridSep * (j + 1),
			gridSep * i, it->second[1], gridSep * (j + 1) }); // Ceiling

		glColor3f(0.2, 0.2, 0.2); // Dark Gray
		DrawQuad({ gridSep * i, it->second[0], gridSep * j,
			gridSep * (i + 1), it->second[0], gridSep * j,
			gridSep * (i + 1), it->second[0], gridSep * (j + 1),
			gridSep * i, it->second[0], gridSep * (j + 1) }); // Floor

		glColor3f(0.5, 0.5, 0.5); // Gray
		//glColor3f(0.5, 0.0, 0.5); // Purple
		f = floatpair(i, j - 1); // Left
		if (levelmap.find(f) == levelmap.end()) {
			if (it->second[1] != noCeilingHeight) // Draw walls between floor and ceiling
				DrawQuad({ gridSep * i, it->second[0], gridSep * j,
					gridSep * (i + 1), it->second[0], gridSep * j,
					gridSep * (i + 1), it->second[1], gridSep * j,
					gridSep * i, it->second[1], gridSep * j });
		} else {
			if (it->second[0] < levelmap[f][0]) // Draw wall between floors
				DrawQuad({ gridSep * i, it->second[0], gridSep * j,
					gridSep * (i + 1), it->second[0], gridSep * j,
					gridSep * (i + 1), levelmap[f][0], gridSep * j,
					gridSep * i, levelmap[f][0], gridSep * j });
			if (it->second[1] > levelmap[f][1] && it->second[1] != noCeilingHeight) // Draw wall between ceilings
				DrawQuad({ gridSep * i, it->second[1], gridSep * j,
					gridSep * (i + 1), it->second[1], gridSep * j,
					gridSep * (i + 1), levelmap[f][1], gridSep * j,
					gridSep * i, levelmap[f][1], gridSep * j });
		}

		//glColor3f(0.0, 0.0, 0.7); // Blue
		f = floatpair(i, j + 1); // Right
		if (levelmap.find(f) == levelmap.end()) {
			if (it->second[1] != noCeilingHeight) // Draw walls between floor and ceiling
				DrawQuad({ gridSep * i, it->second[0], gridSep * (j + 1),
					gridSep * (i + 1), it->second[0], gridSep * (j + 1),
					gridSep * (i + 1), it->second[1], gridSep * (j + 1),
					gridSep * i, it->second[1], gridSep * (j + 1) });
		} else {
			if (it->second[0] < levelmap[f][0]) // Draw wall between floors
				DrawQuad({ gridSep * i, it->second[0], gridSep * (j + 1),
					gridSep * (i + 1), it->second[0], gridSep * (j + 1),
					gridSep * (i + 1), levelmap[f][0], gridSep * (j + 1),
					gridSep * i, levelmap[f][0], gridSep * (j + 1) });
			if (it->second[1] > levelmap[f][1] && it->second[1] != noCeilingHeight) // Draw wall between ceilings
				DrawQuad({ gridSep * i, it->second[1], gridSep * (j + 1),
					gridSep * (i + 1), it->second[1], gridSep * (j + 1),
					gridSep * (i + 1), levelmap[f][1], gridSep * (j + 1),
					gridSep * i, levelmap[f][1], gridSep * (j + 1) });
		}

		//glColor3f(0.0, 0.8, 0.0); // Green
		f = floatpair(i - 1, j); // Up
		if (levelmap.find(f) == levelmap.end()) {
			if (it->second[1] != noCeilingHeight) // Draw walls between floor and ceiling
				DrawQuad({ gridSep * i, it->second[0], gridSep * j,
					gridSep * i, it->second[0], gridSep * (j + 1),
					gridSep * i, it->second[1], gridSep * (j + 1),
					gridSep * i, it->second[1], gridSep * j });
		} else {
			if (it->second[0] < levelmap[f][0]) // Draw wall between floors
				DrawQuad({ gridSep * i, it->second[0], gridSep * j,
					gridSep * i, it->second[0], gridSep * (j + 1),
					gridSep * i, levelmap[f][0], gridSep * (j + 1),
					gridSep * i, levelmap[f][0], gridSep * j });
			if (it->second[1] > levelmap[f][1] && it->second[1] != noCeilingHeight) // Draw wall between ceilings
				DrawQuad({ gridSep * i, it->second[1], gridSep * j,
					gridSep * i, it->second[1], gridSep * (j + 1),
					gridSep * i, levelmap[f][1], gridSep * (j + 1),
					gridSep * i, levelmap[f][1], gridSep * j });
		}

		//glColor3f(1.0, 0.5, 0.0); // Orange
		f = floatpair(i + 1, j); // Down
		if (levelmap.find(f) == levelmap.end()) {
			if (it->second[1] != noCeilingHeight) // Draw walls between floor and ceiling
				DrawQuad({ gridSep * (i + 1), it->second[0], gridSep * j,
					gridSep * (i + 1), it->second[0], gridSep * (j + 1),
					gridSep * (i + 1), it->second[1], gridSep * (j + 1),
					gridSep * (i + 1), it->second[1], gridSep * j });
		} else {
			if (it->second[0] < levelmap[f][0]) // Draw wall between floors
				DrawQuad({ gridSep * (i + 1), it->second[0], gridSep * j,
					gridSep * (i + 1), it->second[0], gridSep * (j + 1),
					gridSep * (i + 1), levelmap[f][0], gridSep * (j + 1),
					gridSep * (i + 1), levelmap[f][0], gridSep * j });
			if (it->second[1] > levelmap[f][1] && it->second[1] != noCeilingHeight) // Draw wall between ceilings
				DrawQuad({ gridSep * (i + 1), it->second[1], gridSep * j,
					gridSep * (i + 1), it->second[1], gridSep * (j + 1),
					gridSep * (i + 1), levelmap[f][1], gridSep * (j + 1),
					gridSep * (i + 1), levelmap[f][1], gridSep * j });
		}
	}
}

// The complete display function. Gets called each frame
void Display() {
	// Updates fps
	fps += 1.0;
	frames2 += 1;
	if (frames2 == prevListStep)
		frames2 = 0;

	// Debug info
	char buffer[1024];
	sprintf_s(buffer, "Pos = %.2f, %.2f, %.2f; Angles = %.2f, %.2f; Velocity = %.1f, %.1f, %.1f; Direction = %d, %d; Ground = %.1f", posX, posY, posZ, angleX, angleY, velocityX, velocityY, velocityZ, direction.first, direction.second, groundHeight);
	glutSetWindowTitle(buffer);

	// Checks inputs
	CheckInputs();

	// Initialize drawing
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();

	///// Begin 3D
	glPushMatrix();
	glRotatef(angleY, 1.0f, 0.0f, 0.0f);
	glRotatef(angleX, 0.0f, 1.0f, 0.0f);
	glRotatef(180.0, 0.0f, 0.0f, 1.0f);
	glTranslatef(posX, posY, posZ);

	// Draws the maps
	// TODO: drawing multiple maps is not doable. Better: load and unload single maps
	//for (LevelMap* map : activeMaps) {
	//	DrawMap(map->layout);
	//}
	DrawMap(currentMap->layout);

	// Updates and draws prevPosList
	glColor3f(1.0, 0.0, 0.0); // Red
	if (frames2 == 0) {
		prevPosList.pop_front();
		prevPosList.push_back(new Vector3(posX, posY, posZ));
	}
	if (showRedLine) {
		DrawPosDeq(prevPosList);
	}

	///// End 3D
	glPopMatrix();

	///// Begin 2D
	glPushMatrix();
	glTranslatef(0.0, 0.0, -fMin);

	// Draws HUD
	glColor3f(0.5, 0.0, 0.5); // Purple
	char jump[100], crouch[100], fly[100], air[100], red[100];
	DisplayVar("Jumping = ", jumping, jump);
	DisplayVar("Crouching = ", crouching, crouch);
	DisplayVar("Flying = ", flying, fly);
	DisplayVar("In Air = ", inAir, air);
	DisplayVar("Red Line = ", showRedLine, red);
	stringvec hud = { jump, crouch, fly, air, red };
	DrawText2Pix(hud, 20, 20);

	// Draws crosshairs
	glColor3f(0.0, 1.0, 0.0); // Green
	DrawCrosshairs(5, 3);

	///// End 2D
	glPopMatrix();

	// Swaps buffers
	glutSwapBuffers();
}

// When the display window is reshaped
void Reshape(int width, int height) {
	glViewport(0, 0, (GLsizei)width, (GLsizei)height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(alpha, (GLfloat)width / (GLfloat)height, fMin, fMax);
	glMatrixMode(GL_MODELVIEW);
}

// Main function
int main(int iArgc, char** cppArgv) {
	// Creates all LevelMaps
	LevelMap level1, level2;
	level1 = LevelMap("Level1.txt", 0);
	level2 = LevelMap("Level1.txt", 0);
	activeMaps.push_back(&level1);
	activeMaps.push_back(&level2);

	// Links all LevelMaps
	level1.AddConnection(Vector3(9, 2.5, 9), 0, &level2, Vector3(17, 0, 13), 270);

	// Determines the current LevelMap based on posX and posZ
	currentMap = &level1; // GetCurrentMap(posX, posZ);

	// Sets starting position posY
	float addHeight = GetGroundHeight(posX, posZ, currentMap);
	if (addHeight != noCeilingHeight)
		posY = playerHeight + addHeight;

	// Initiates keyTable and keyHandled
	for (int i = 0; i < 256; i++) {
		keyTable[i] = false;
		keyHandled[i] = false;
	}

	// Initiates prevPosList
	for (int i = 0; i < prevListCount; i++) {
		prevPosList.push_back(new Vector3(posX, posY, posZ));
	}

	//texture = LoadTexture("./FloorTile.bmp");

	glutInit(&iArgc, cppArgv); // Initializes glut

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH); // Sets the parameters for GLUT
	glutInitWindowSize(screenW, screenH); // Initializes the window size
	glutInitWindowPosition((1920 - screenW) / 2, (1080 - screenH) / 2); // Initializes the window position

	glutCreateWindow("Example"); // Sets the window name

	glEnable(GL_DEPTH_TEST);

	glutSetCursor(GLUT_CURSOR_NONE);
	glutWarpPointer(centerX, centerY);

	glutDisplayFunc(Display);
	glutIdleFunc(Display);
	glutReshapeFunc(Reshape);
	glutKeyboardFunc(KeyPressed);
	glutKeyboardUpFunc(KeyUp);
	glutMotionFunc(MouseMovement);
	glutPassiveMotionFunc(MouseMovement);
	glutTimerFunc(1000, SecondPassed, 0);

	glutMainLoop(); // Runs the program using the callback functions that were specified before, never returns

	return 0;
}