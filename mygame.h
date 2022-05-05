#pragma once

namespace Tmpl8
{

class MyGame : public Game
{
	void PrintDebug();
	void PrintStats();
	LightManager lightManager;
public:
	LightManager& GetLightManager() { return lightManager; }
	void SetupReservoirBuffers();
	// game flow methods
	void Init();
	void Tick( float deltaTime );
	void Shutdown() { /* implement if you want to do something on exit */ }
	// input handling 
	void MouseUp( int button ) { /* implement if you want to detect mouse button presses */ }
	void MouseDown( int button ) { /* implement if you want to detect mouse button presses */ }
	void MouseMove( int x, int y ) { mousePos.x = x, mousePos.y = y; }
	void KeyUp( int key ) { /* implement if you want to handle keys */ }
	void KeyDown( int key ) { /* implement if you want to handle keys */ }
	// data members
	int2 mousePos;

	void DumpScreenBuffer();

	void HandleControls(float deltaTime);
	void PreRender();

	//wrapper for void(MyGame, int) fn
	static void IntArgFunction(std::function<void(MyGame&, int)> fn, MyGame& g, std::string s, int defaultarg);
};

} // namespace Tmpl8