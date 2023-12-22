#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <Windows.h>

class Raycaster {
public:
	Raycaster();
	~Raycaster();

	void Run();

private:
	void Initialize();
	void Update();
	void Render();
	void CastRays();
	void HandleMovement(float frameElapsedTime);
	void HandleRotation(float frameElapsedTime);
	void HandleWallIntersection(int testX, int testY, float distanceToWall, bool& hitWall, bool& boundary, float rayAngle);

private:
	int screenWidth;        /* Console Screen Size X (columns) */
	int screenHeight;       /* Console Screen Size Y (rows) */
	int mapWidth;           /* World Dimensions */
	int mapHeight;
	float playerX;          /* Player Start Position X */
	float playerY;          /* Player Start Position Y */
	float playerA;          /* Player Start Rotation - the angle player is looking at */
	float fov;              /* Field of View */
	float depth;            /* Maximum rendering distance */
	float speed;            /* Walking Speed */
	std::string gameMap;
	wchar_t* screenBuffer;
	HANDLE consoleHandle;
};

Raycaster::Raycaster()
	:
	screenWidth(120),
	screenHeight(40),
	mapWidth(16),
	mapHeight(16),
	playerX(14.7f),
	playerY(5.09f),
	playerA(0.0f),
	fov(3.14159f / 3.0f),
	depth(16.0f),
	speed(5.0f),
	screenBuffer(new wchar_t[screenWidth * screenHeight]),
	consoleHandle(CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL))
{
	SetConsoleActiveScreenBuffer(consoleHandle);
	gameMap = "#########......."
		"#..............."
		"#.......########"
		"#..............#"
		"#......##......#"
		"#......##......#"
		"#......##......#"
		"#......#########"
		"#..............#"
		"#..............#"
		"#..............#"
		"#######....#####"
		"#..............#"
		"#..............#"
		"#..............#"
		"################";
}

Raycaster::~Raycaster()
{
	delete[] screenBuffer;
}

void Raycaster::Run()
{
	Initialize();

	auto timePoint1 = std::chrono::system_clock::now();
	auto timePoint2 = std::chrono::system_clock::now();

	while (1) {
		Render();
		/* Frame-to-frame time difference used for adjusting
		movement speeds to ensure smooth and consistent motion,
		especially crucial in ray-tracing due to its non-deterministic nature. */
		timePoint2 = std::chrono::system_clock::now();
		std::chrono::duration<float> elapsedTime = timePoint2 - timePoint1;
		timePoint1 = timePoint2;
		float frameElapsedTime = elapsedTime.count();

		HandleRotation(frameElapsedTime);
		HandleMovement(frameElapsedTime);
		CastRays();
	}
}

void Raycaster::Initialize()
{
	DWORD dwBytesWritten = 0;
	for (int i = 0; i < screenWidth * screenHeight; ++i) {
		screenBuffer[i] = ' ';
	}

	WriteConsoleOutputCharacter(consoleHandle, screenBuffer, screenWidth * screenHeight, { 0, 0 }, &dwBytesWritten);
}


void Raycaster::Render()
{
	DWORD dwBytesWritten = 0;
	screenBuffer[screenWidth * screenHeight - 1] = '\0';
	WriteConsoleOutputCharacter(consoleHandle, screenBuffer, screenWidth * screenHeight, { 0, 0 }, &dwBytesWritten);
}

void Raycaster::CastRays()
{
	for (int x = 0; x < screenWidth; x++) {
		/* Calculate the projected ray angle into world space for each column */
		float rayAngle = (playerA - fov / 2.0f) + ((float)x / (float)screenWidth) * fov;

		/* Determine distance to wall
		Adjust step size for ray casting; decrease for higher resolution */
		float stepSize = 0.01f;
		float distanceToWall = 0.0f;

		bool hitWall = false; /* Flag indicating when the ray hits a wall block */
		bool boundary = false; /* Flag indicating when the ray hits the boundary between two wall blocks */

		float eyeX = sinf(rayAngle); /* Unit vector representing the ray in player space */
		float eyeY = cosf(rayAngle);

		/* Incrementally cast the ray from the player along the ray angle, checking for
		intersection with a block */
		while (!hitWall && distanceToWall < depth) {
			distanceToWall += stepSize;
			int testX = static_cast<int>(playerX + eyeX * distanceToWall);
			int testY = static_cast<int>(playerY + eyeY * distanceToWall);

			/* Test if ray is out of bounds */
			if (testX < 0 || testX >= mapWidth || testY < 0 || testY >= mapHeight) {
				hitWall = true;
				distanceToWall = depth;
			}
			else {
				/* Ray is in bounds so test to see if the ray cell is a wall block */
				HandleWallIntersection(testX, testY, distanceToWall, hitWall, boundary, rayAngle);
			}
		}

		/* Calculate distance to ceiling and floor */
		int ceiling = static_cast<int>((screenHeight / 2.0) - screenHeight / distanceToWall);
		int floor = screenHeight - ceiling;

		/* Shader walls based on distance */
		short shade = ' ';
		if (distanceToWall <= depth / 4.0f) {
			shade = 0x2588;
		}
		else if (distanceToWall < depth / 3.0f) {
			shade = 0x2593;
		}
		else if (distanceToWall < depth / 2.0f) {
			shade = 0x2592;
		}
		else if (distanceToWall < depth) {
			shade = 0x2591;
		}
		else {
			shade = ' '; /* Too far away */
		}

		if (boundary) {
			shade = ' '; /* Black it out */
		}

		for (int y = 0; y < screenHeight; y++) {
			if (y <= ceiling) {
				screenBuffer[y * screenWidth + x] = ' ';
			}
			else if (y > ceiling && y <= floor) {
				screenBuffer[y * screenWidth + x] = shade;
			}
			else {
				/* Floor */
				float b = 1.0f - (((float)y - screenHeight / 2.0f) / ((float)screenHeight / 2.0f));
				if (b < 0.20) {
					shade = '#';
				}
				else if (b < 0.4) {
					shade = 'x';
				}
				else if (b < 0.60) {
					shade = '.';
				}
				else if (b < 0.8) {
					shade = '-';
				}
				else {
					shade = ' ';
				}
				screenBuffer[y * screenWidth + x] = shade;
			}
		}
	}
}

void Raycaster::HandleMovement(float frameElapsedTime)
{
	// Handle forwards movement and collision
	if (GetAsyncKeyState((unsigned short)'W') & 0x8000) {
		playerX += sinf(playerA) * speed * frameElapsedTime;
		playerY += cosf(playerA) * speed * frameElapsedTime;
		if (gameMap.c_str()[(int)playerX * mapWidth + (int)playerY] == '#') {
			playerX -= sinf(playerA) * speed * frameElapsedTime;
			playerY -= cosf(playerA) * speed * frameElapsedTime;
		}
	}

	// Handle backwards movement and collision
	if (GetAsyncKeyState((unsigned short)'S') & 0x8000) {
		playerX -= sinf(playerA) * speed * frameElapsedTime;
		playerY -= cosf(playerA) * speed * frameElapsedTime;
		if (gameMap.c_str()[(int)playerX * mapWidth + (int)playerY] == '#') {
			playerX += sinf(playerA) * speed * frameElapsedTime;
			playerY += cosf(playerA) * speed * frameElapsedTime;
		}
	}
}

void Raycaster::HandleRotation(float frameElapsedTime)
{
	if (GetAsyncKeyState((unsigned short)'A') & 0x8000)
		playerA -= (speed * 0.35f) * frameElapsedTime;

	if (GetAsyncKeyState((unsigned short)'D') & 0x8000)
		playerA += (speed * 0.35f) * frameElapsedTime;
}

void Raycaster::HandleWallIntersection(int testX, int testY, float distanceToWall, bool& hitWall, bool& boundary, float rayAngle)
{
	if (gameMap.c_str()[testX * mapWidth + testY] == '#') {
		hitWall = true;

		/* To emphasize tile boundaries, cast rays from each corner
		of the tile to the player. The closer the alignment with the rendering ray,
		the nearer the tile boundary, which we'll shade to enhance wall detail */
		std::vector<std::pair<float, float>> p;
		float eyeX = sinf(rayAngle); /* Unit vector representing the ray in player space */
		float eyeY = cosf(rayAngle);

		/* Test each corner of hit tile, storing the distance from
		the player, and the calculated dot product of the two rays */
		for (int tileX = 0; tileX < 2; tileX++)
			for (int tileY = 0; tileY < 2; tileY++) {
				/* Angle of corner to eye */
				float vy = (float)testY + tileY - playerY;
				float vx = (float)testX + tileX - playerX;
				float d = sqrt(vx * vx + vy * vy);
				float dot = (eyeX * vx / d) + (eyeY * vy / d);
				p.push_back(std::make_pair(d, dot));
			}

		/* Sort Pairs from closest to farthest */
		std::sort(p.begin(), p.end(), [](const std::pair<float, float>& left, const std::pair<float, float>& right) {
			return left.first < right.first;
			});

		/* First two/three are closest (we will never see all four) */
		float bound = 0.01f;
		if (acos(p.at(0).second) < bound || acos(p.at(1).second) < bound || acos(p.at(2).second) < bound) {
			boundary = true;
		}
	}
}

int main() {
	Raycaster raycaster;
	raycaster.Run();

	return 0;
}