#include "common.h"
#include "inipp.h"
#include <fstream>

#define CONFIG_FILE ".\\config.ini"

#define UPDATE_INTERVAL 500

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

	if (hFile == INVALID_HANDLE_VALUE)
	{
		hFile = CreateFile(
			filePath,
			GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			CREATE_NEW,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);
	}

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

	if (lastCheckTime != 0 && now < lastCheckTime + UPDATE_INTERVAL)
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

	settings.blurPasses = blur;
	settings.blurDownscale = blurSize;
	settings.updateInterval = updateInt;
	settings.mirrored = mirrored;


	std::string currentRes = "";
	inipp::get_value(ini.sections["Game"], "Resolution", currentRes);

	settings.resolutions.current = currentRes;
	settings.resolutions.available.clear();

	for (auto const& sec : ini.sections)
	{
		std::string name = sec.first;

		if (name == "Game")
			continue;

		int res_width = 0;
		int res_height = 0;
		bool res_aspect = false;

		inipp::get_value(ini.sections[name.c_str()], "Width", res_width);
		inipp::get_value(ini.sections[name.c_str()], "Height", res_height);
		inipp::get_value(ini.sections[name.c_str()], "IsAspectRatio", res_aspect);

		//settings.resolutions
		ResolutionSettings rs = {};
		rs.name = name;
		rs.width = res_width;
		rs.height = res_height;
		rs.isAspectRatio = res_aspect;

		settings.resolutions.available.push_back(rs);

		if (name == currentRes)
		{
			settings.gameWidth = res_width;
			settings.gameHeight = res_height;
			settings.isAspectRatio = res_aspect;
		}
	}

	return true;
}

void SaveSettings(AppSettings& settings)
{
	// config file
	inipp::Ini<char> ini;
	std::ifstream is("config.ini");
	ini.parse(is);
	ini.strip_trailing_comments();

	ini.sections["Game"]["Resolution"] = settings.resolutions.current;
	ini.sections["Game"]["BlurStrength"] = std::to_string(settings.blurPasses);
	ini.sections["Game"]["BlurDownscale"] = std::to_string(settings.blurDownscale);
	ini.sections["Game"]["UpdateInterval"] = std::to_string(settings.updateInterval);
	ini.sections["Game"]["Mirrored"] = settings.mirrored ? "true" : "false";

	for (auto& res : settings.resolutions.available)
	{
		ini.sections[res.name]["Width"] = std::to_string(res.width);
		ini.sections[res.name]["Height"] = std::to_string(res.height);
		ini.sections[res.name]["IsAspectRatio"] = res.isAspectRatio ? "true" : "false";
	}

	is.close();

	std::ofstream os("config.ini");
	ini.generate(os);
	os.close();
}