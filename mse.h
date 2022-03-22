#pragma once

namespace Tmpl8
{
	class Image
	{
	public:
		float4* buffer = 0;
		uint width = 0, height = 0;

		static void Load(std::filesystem::path, Image& image);
	};

class MSE : public Game
{
public:
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
	void Render(Surface* surface);

	void HandleControls(float deltaTime);
};

} // namespace Tmpl8