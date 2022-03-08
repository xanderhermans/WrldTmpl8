#include "precomp.h"
#include "mygame.h"
#include "mygamescene.h"

Game* CreateGame() { return new MyGame(); }

float3 D(0.75, -0.36, 0.56), O(21.52, 108.43, 26.60);
// -----------------------------------------------------------
// Initialize the application
// -----------------------------------------------------------
void MyGame::Init()
{
	MyGameScene::CreateWorld(*GetWorld());
	
	WorldToOBJ(GetWorld(), "scene_export/scene");
}

void handle_controls(float deltaTime)
{
	// free cam controls
	float3 tmp(0, 1, 0), right = normalize(cross(tmp, D)), up = cross(D, right);
	float speed = deltaTime * 0.1f;
	if (GetAsyncKeyState('W')) O += speed * D; else if (GetAsyncKeyState('S')) O -= speed * D;
	if (GetAsyncKeyState('A')) O -= speed * right; else if (GetAsyncKeyState('D')) O += speed * right;
	if (GetAsyncKeyState('R')) O += speed * up; else if (GetAsyncKeyState('F')) O -= speed * up;
	if (GetAsyncKeyState(VK_LEFT)) D = normalize(D - right * 0.025f * speed);
	if (GetAsyncKeyState(VK_RIGHT)) D = normalize(D + right * 0.025f * speed);
	if (GetAsyncKeyState(VK_UP)) D = normalize(D - up * 0.025f * speed);
	if (GetAsyncKeyState(VK_DOWN)) D = normalize(D + up * 0.025f * speed);
	LookAt(O, O + D);
}

// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void MyGame::Tick( float deltaTime )
{
	handle_controls(deltaTime);
	//printf("                                                             \r");
	//printf("Camera at x:%.2f, y:%.2f, z:%.2f direction %.2f:%.2f:%.2f", O.x, O.y, O.z, D.x, D.y, D.z);
}