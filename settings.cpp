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

    int blur = DEFAULT_BLUR_PASSES;
    inipp::get_value(ini.sections["Game"], "BlurStrength", blur);

    int blurSize = DEFAULT_BLUR_DOWNSCALE;
    inipp::get_value(ini.sections["Game"], "BlurDownscale", blurSize);

    int blurSamples = DEFAULT_BLUR_SAMPLES;
    inipp::get_value(ini.sections["Game"], "BlurSamples", blurSamples);

    int frameRate = DEFAULT_FRAMERATE;
    inipp::get_value(ini.sections["Game"], "FrameRate", frameRate);

    bool mirrored = DEFAULT_MIRRORED;
    inipp::get_value(ini.sections["Game"], "Mirrored", mirrored);

    bool stretched = DEFAULT_STRETCHED;
    inipp::get_value(ini.sections["Game"], "Stretched", stretched);

    int zoom = DEFAULT_ZOOM;
    inipp::get_value(ini.sections["Game"], "Zoom", zoom);

    bool vignetteEnabled = DEFAULT_VIGNETTE_ENABLED;
    inipp::get_value(ini.sections["Game"], "VignetteEnabled", vignetteEnabled);

    float vignetteIntensity = DEFAULT_VIGNETTE_INTENSITY;
    inipp::get_value(ini.sections["Game"], "VignetteIntensity", vignetteIntensity);

    float vignetteRadius = DEFAULT_VIGNETTE_RADIUS;
    inipp::get_value(ini.sections["Game"], "VignetteRadius", vignetteRadius);

    float vignetteSmoothness = DEFAULT_VIGNETTE_SMOOTHNESS;
    inipp::get_value(ini.sections["Game"], "VignetteSmoothness", vignetteSmoothness);

    bool useAuto = DEFAULT_AUTO_DETECTION;
    inipp::get_value(ini.sections["Game"], "AutoDetection", useAuto);

    int autoDetectionTime = DEFAULT_AUTO_DETECTION_TIME;
    inipp::get_value(ini.sections["Game"], "AutoDetectionTime", autoDetectionTime);

    float autoDetectionBrightnessThreshold = DEFAULT_AUTO_DETECTION_BRIGHTNESS_THRESHOLD;
    inipp::get_value(ini.sections["Game"], "AutoDetectionBrightnessThreshold", autoDetectionBrightnessThreshold);

    float autoDetectionBlackRatio = DEFAULT_AUTO_DETECTION_BLACK_RATIO;
    inipp::get_value(ini.sections["Game"], "AutoDetectionBlackRatio", autoDetectionBlackRatio);


    settings.blurPasses = blur;
    settings.blurDownscale = blurSize;
    settings.blurSamples = blurSamples;
    settings.frameRate = frameRate;
    settings.mirrored = mirrored;
    settings.stretched = stretched;
    settings.zoom = zoom;
    settings.vignetteEnabled = vignetteEnabled;
    settings.vignetteIntensity = vignetteIntensity;
    settings.vignetteRadius = vignetteRadius;
    settings.vignetteSmoothness = vignetteSmoothness;
    settings.useAutoDetection = useAuto;
    settings.autoDetectionTime = autoDetectionTime;
    settings.autoDetectionBrightnessThreshold = autoDetectionBrightnessThreshold;
    settings.autoDetectionBlackRatio = autoDetectionBlackRatio;

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


        inipp::get_value(ini.sections[name.c_str()], "Width", res_width);
        inipp::get_value(ini.sections[name.c_str()], "Height", res_height);

        //settings.resolutions
        ResolutionSettings rs = {};
        rs.name = name;
        rs.width = res_width;
        rs.height = res_height;

        settings.resolutions.available.push_back(rs);

        if (name == currentRes)
        {
            settings.gameWidth = res_width;
            settings.gameHeight = res_height;
        }
    }

    // if ini config is missing, add some default sections
    if (settings.resolutions.available.size() == 0)
    {
        ResolutionSettings rs = {};
        rs.name = "16:9";
        rs.width = 16;
        rs.height = 9;
        settings.resolutions.available.push_back(rs);
        settings.resolutions.current = "16:9";

        rs.name = "4:3";
        rs.width = 4;
        rs.height = 3;
        settings.resolutions.available.push_back(rs);

        rs.name = "21:9";
        rs.width = 21;
        rs.height = 9;
        settings.resolutions.available.push_back(rs);
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
    ini.sections["Game"]["BlurSamples"] = std::to_string(settings.blurSamples);
    ini.sections["Game"]["FrameRate"] = std::to_string(settings.frameRate);
    ini.sections["Game"]["Mirrored"] = settings.mirrored ? "true" : "false";
    ini.sections["Game"]["Stretched"] = settings.stretched ? "true" : "false";
    ini.sections["Game"]["Zoom"] = std::to_string(settings.zoom);
    ini.sections["Game"]["VignetteEnabled"] = settings.vignetteEnabled ? "true" : "false";
    ini.sections["Game"]["VignetteIntensity"] = std::to_string(settings.vignetteIntensity);
    ini.sections["Game"]["VignetteRadius"] = std::to_string(settings.vignetteRadius);
    ini.sections["Game"]["VignetteSmoothness"] = std::to_string(settings.vignetteSmoothness);
    ini.sections["Game"]["AutoDetection"] = settings.useAutoDetection ? "true" : "false";
    ini.sections["Game"]["AutoDetectionTime"] = std::to_string(settings.autoDetectionTime);
    ini.sections["Game"]["AutoDetectionBrightnessThreshold"] = std::to_string(settings.autoDetectionBrightnessThreshold);
    ini.sections["Game"]["AutoDetectionBlackRatio"] = std::to_string(settings.autoDetectionBlackRatio);


    for (auto& res : settings.resolutions.available)
    {
        ini.sections[res.name]["Width"] = std::to_string(res.width);
        ini.sections[res.name]["Height"] = std::to_string(res.height);
    }

    is.close();

    std::ofstream os("config.ini");
    ini.generate(os);
    os.close();
}
