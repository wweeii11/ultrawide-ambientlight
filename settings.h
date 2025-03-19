#include <vector>
#include <string>

#define DEFAULT_BLUR		3
#define DEFAULT_DOWNSCALE	64
#define DEFAULT_FRAMERATE	30
#define DEFAULT_MIRRORED	false

struct ResolutionSettings
{
    std::string name = "";
    UINT width = 0;
    UINT height = 0;
    bool isAspectRatio = false;
};

struct Resolutions
{
    std::string current;
    std::vector<ResolutionSettings> available;
};

struct AppSettings
{
    UINT gameWidth = 0;
    UINT gameHeight = 0;
    bool isAspectRatio = false;
    UINT blurDownscale = 0;
    UINT blurPasses = 0;
    UINT frameRate = 30;
    bool mirrored = false;
    Resolutions resolutions;
};



bool ReadSettings(AppSettings& settings);
void SaveSettings(AppSettings& settings);
