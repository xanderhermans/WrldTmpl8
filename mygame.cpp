#include "precomp.h"
#include "mygame.h"
#include "mygamescene.h"

Game* CreateGame() { return new MyGame(); }

//float3 _D(0.75, -0.36, 0.56), _O(21.52, 108.43, 26.60);
float3 _D(0.11, -0.22, 0.97), _O(107.85, 60.81, -48.86);
float3 D = _D, O = _O;
// -----------------------------------------------------------
// Initialize the application
// -----------------------------------------------------------
void MyGame::Init()
{
	MyGameScene::CreateWorld(*GetWorld());
	//autoRendering = false;
	WorldToOBJ(GetWorld(), "scene_export/scene");
}

void MyGame::HandleControls(float deltaTime)
{
	if (!isFocused) return; // ignore controls if window doesnt have focus
	// free cam controls
	float3 tmp(0, 1, 0), right = normalize(cross(tmp, D)), up = cross(D, right);
	float speed = deltaTime * 0.1f;
	bool dirty = false;

	if (GetAsyncKeyState('W')) { O += speed * D; dirty = true; }
	else if (GetAsyncKeyState('S')) { O -= speed * D; dirty = true; }
	if (GetAsyncKeyState('A')) { O -= speed * right; dirty = true; }
	else if (GetAsyncKeyState('D')) { O += speed * right; dirty = true; }

	if (GetAsyncKeyState('R')) { O += speed * up; dirty = true; }
	else if (GetAsyncKeyState('F')) { O -= speed * up; dirty = true; }

	if (GetAsyncKeyState(VK_LEFT)) { D = normalize(D - right * 0.025f * speed); dirty = true; }
	else if (GetAsyncKeyState(VK_RIGHT)) { D = normalize(D + right * 0.025f * speed); dirty = true; }
	if (GetAsyncKeyState(VK_UP)) { D = normalize(D - up * 0.025f * speed); dirty = true; }
	else if (GetAsyncKeyState(VK_DOWN)) { D = normalize(D + up * 0.025f * speed); dirty = true; }
	
	if (GetAsyncKeyState('Z')) { D = _D; O = _O; dirty = true; }
	
	LookAt(O, O + D);

	if (GetAsyncKeyState(VK_SPACE)) GetWorld()->accumulate = !GetWorld()->accumulate;
	if (GetAsyncKeyState('X')) dirty = true;
	GetWorld()->dirty = dirty;
}

void printStats()
{
	printf("                                                            \r");
	printf("Frame: %d Camera at _D(%.2f, %.2f, %.2f), _O(%.2f, %.2f, %.2f);", GetWorld()->GetRenderParams().framecount, D.x, D.y, D.z, O.x, O.y, O.z);
}

// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void MyGame::Tick(float deltaTime)
{
	HandleControls(deltaTime);
	//clWaitForEvents(1,&GetWorld()->GetRenderDoneEventHandle());
	//GetWorld()->GetFrameBuffer()->CopyFromDevice();
	//auto buf = GetWorld()->GetFrameBuffer()->GetHostPtr();

	printStats();
}