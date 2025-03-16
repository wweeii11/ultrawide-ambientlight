#define DEFAULT_BLUR		3
#define DEFAULT_DOWNSCALE	64
#define DEFAULT_UPDATE		2
#define DEFAULT_MIRRORED	false

struct AppSettings
{
	UINT gameWidth = 0;
	UINT gameHeight = 0;
	bool useAspectRatio = false;
	UINT blurDownscale = 0;
	UINT blurPasses = 0;
	UINT updateInterval = 0;
	bool mirrored = false;
};

bool ReadSettings(AppSettings& settings);
