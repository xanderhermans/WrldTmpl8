// definition of the C-API of the Voxel World Template

World* GetWorld();
void ClearWorld();
void WorldXScroll( const int offset );
void WorldYScroll( const int offset );
void WorldZScroll( const int offset );
void Plot( const uint x, const uint y, const uint z, const uint c );
void Plot( const uint3 pos, const uint c );
void Plot( const int3 pos, const uint c );
uint Read( const int x, const int y, const int z );
uint Read( const int3 pos );
uint Read( const uint3 pos );
void Sphere( const float x, const float y, const float z, const float r, const uint c );
void Sphere( const float3 pos, const float r, const uint c );
void Box( const int x1, const int y1, const int z1, const int x2, const int y2, const int z2, const uint c );
void Box( const int3 pos1, const int3 pos2, const uint c );
void Copy( const int3 s1, const int3 s2, const int3 D );
void Copy( const int3 s1, const int3 s2, const int x, const int y, const int z );
void Copy( const int x1, const int y1, const int z1, const int x2, const int y2, const int z2, const int3 D );
void Copy( const int x1, const int y1, const int z1, const int x2, const int y2, const int z2, const uint3 D );
void HDisc( const float x, const float y, const float z, const float r, const uint c );
void HDisc( const float3 pos, const float r, const uint c );
void Print( const char* text, const uint x, const uint y, const uint z, const uint c );
void Print( const char* text, const int3 pos, const uint c );
void Print( const char* text, const uint3 pos, const uint c );
uint LoadSprite( const char* voxFile, bool palShift = true );
uint CloneSprite( const uint idx );
uint CreateSprite( const int3 pos, const int3 size, const int frames = 1 );
uint CreateSprite( const int x, const int y, const int z, const int w, const int h, const int d, const int frames = 1 );
int3 GetSpriteFrameSize( const uint idx );
uint GetSpriteVoxel( const uint idx, const int3 pos );
uint GetSpriteVoxel( const uint idx, const uint3 pos );
uint GetSpriteVoxel( const uint idx, const uint x, const uint y, const uint z );
uint CreateParticles( const uint count );
void SetParticle( const uint set, const uint idx, const int3 pos, const uint v );
void SetParticle( const uint set, const uint idx, const uint3 pos, const uint v );
void SetParticle( const uint set, const uint idx, const uint x, const uint y, const uint z, const uint v );
void EnableShadow( const uint idx );
void DisableShadow( const uint idx );
void SetSpriteFrame( const uint idx, const uint frame );
bool SpriteHit( const uint A, const uint B );
void MoveSpriteTo( const uint idx, const uint x, const uint y, const uint z );
void MoveSpriteTo( const uint idx, const int3 pos );
void MoveSpriteTo( const uint idx, const uint3 pos );
void RemoveSprite( const uint idx );
uint LoadTile( const char* voxFile );
uint LoadBigTile( const char* voxFile );
void DrawTile( const uint idx, const uint x, const uint y, const uint z );
void DrawTile( const uint idx, const int3 pos );
void DrawTile( const uint idx, const uint3 pos );
void DrawTiles( const char* tileString, const uint x, const uint y, const uint z );
void DrawTiles( const char* tileString, const int3 pos );
void DrawTiles( const char* tileString, const uint3 pos );
void DrawBigTile( const uint idx, const uint x, const uint y, const uint z );
void DrawBigTile( const uint idx, const int3 pos );
void DrawBigTile( const uint idx, const uint3 pos );
void DrawBigTiles( const char* tileString, const uint x, const uint y, const uint z );
void DrawBigTiles( const char* tileString, const int3 pos );
void DrawBigTiles( const char* tileString, const uint3 pos );
void LookAt( const float3 pos, const float3 target );
void LookAt( const int x1, const int y1, const int z1, const int x2, const int y2, const int z2 );
void LookAt( const float x1, const float y1, const float z1, const float x2, const float y2, const float z2 );
void Forward( const float d );
void Forward( const int d );
void Line( const uint x1, const uint y1, const uint z1, const uint x2, const uint y2, const uint z2, const uint c );
void Line( const uint3 A, const uint3 B, const uint c );
void XLine( const uint x, const uint y, const uint z, int l, const uint c );
void XLine( const uint3 pos, int l, const uint c );
void XLine( const int3 pos, int l, const uint c );
void YLine( const uint x, const uint y, const uint z, int l, const uint c );
void YLine( const uint3 pos, int l, const uint c );
void YLine( const int3 pos, int l, const uint c );
void ZLine( const uint x, const uint y, const uint z, int l, const uint c );
void ZLine( const uint3 pos, int l, const uint c );
void ZLine( const int3 pos, int l, const uint c );
bool IsOccluded( const float3 P1, const float3 P2 );
float Trace( const float3 P1, const float3 P2 );
uint RGB32to8( const uint c );
uint BGR32to8( const uint c );

// EOF