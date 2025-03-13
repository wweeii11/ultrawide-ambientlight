#include "app.h"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	AmbientLight ambientLight(3360, 1440);
	PresentWindow present;
	present.Create(hInstance, &ambientLight);
	present.Run();
	return 0;
}
