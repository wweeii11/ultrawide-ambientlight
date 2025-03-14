#include "app.h"
#include "inipp.h"
#include <fstream>

#define DEFAULT_BLUR		3
#define DEFAULT_DOWNSCALE	64
#define DEFAULT_UPDATE		2
#define DEFAULT_MIRRORED	false

AppSettings ReadSettings()
{
	// read config file
	inipp::Ini<char> ini;
	std::ifstream is("config.ini");
	ini.parse(is);
	ini.strip_trailing_comments();

	int blur = DEFAULT_BLUR;
	inipp::get_value(ini.sections["Game"], "BlurStrength", blur);

	int blurSize = DEFAULT_DOWNSCALE;
	inipp::get_value(ini.sections["Game"], "BlurDownscale", blurSize);
	
	int updateInt = DEFAULT_UPDATE;
	inipp::get_value(ini.sections["Game"], "UpdateInterval", updateInt);

	bool mirrored = DEFAULT_MIRRORED;
	inipp::get_value(ini.sections["Game"], "Mirrored", mirrored);

	std::string resolution = "";
	inipp::get_value(ini.sections["Game"], "Resolution", resolution);

	int res_width = 0;
	int res_height = 0;
	if (resolution.length() > 0)
	{
		inipp::get_value(ini.sections[resolution.c_str()], "Width", res_width);
		inipp::get_value(ini.sections[resolution.c_str()], "Height", res_height);
	}

	AppSettings settings = {};
	settings.gameWidth = res_width;
	settings.gameHeight = res_height;
	settings.blurPasses = blur;
	settings.blurDownscale = blurSize;
	settings.updateInterval = updateInt;
	settings.mirrored = mirrored;

	return settings;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	AppSettings settings = ReadSettings();

	AmbientLight ambientLight(settings);
	PresentWindow present;
	present.Create(hInstance, &ambientLight);
	present.Run();
	return 0;
}
