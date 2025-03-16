#include "common.h"
#include "inipp.h"
#include <fstream>

#define CONFIG_FILE ".\\config.ini"

ULONGLONG lastCheckTime = 0;
FILETIME lastWriteTime = {};

FILETIME GetFileLastWriteTime(LPCTSTR filePath)
{
	FILETIME ftWrite = { 0 };

	// Open the file to get its attributes
	HANDLE hFile = CreateFile(
		filePath,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (hFile != INVALID_HANDLE_VALUE) {
		// Get the last write time
		GetFileTime(hFile, NULL, NULL, &ftWrite);
		CloseHandle(hFile);
	}

	return ftWrite;
}

BOOL CompareFileTime(FILETIME first, FILETIME second) {
	return (first.dwLowDateTime == second.dwLowDateTime && first.dwHighDateTime == second.dwHighDateTime);
}

bool ReadSettings(AppSettings& settings)
{
	ULONGLONG now = GetTickCount64();

	if (lastCheckTime != 0 && now < lastCheckTime + 1000)
	{
		return false;
	}

	lastCheckTime = now;

	FILETIME currentWriteTime = GetFileLastWriteTime(CONFIG_FILE);
	if (CompareFileTime(currentWriteTime, lastWriteTime))
	{
		return false;
	}

	lastWriteTime = currentWriteTime;

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
	bool res_aspect = false;
	if (resolution.length() > 0)
	{
		inipp::get_value(ini.sections[resolution.c_str()], "Width", res_width);
		inipp::get_value(ini.sections[resolution.c_str()], "Height", res_height);
		inipp::get_value(ini.sections[resolution.c_str()], "UseAspectRatio", res_aspect);
	}

	settings.gameWidth = res_width;
	settings.gameHeight = res_height;
	settings.useAspectRatio = res_aspect;
	settings.blurPasses = blur;
	settings.blurDownscale = blurSize;
	settings.updateInterval = updateInt;
	settings.mirrored = mirrored;

	return true;
}
