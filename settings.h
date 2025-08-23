#pragma once

#include <vector>
#include <string>

#include "windows.h"

#define DEFAULT_BLUR_SAMPLES        5
#define DEFAULT_BLUR_PASSES		    3
#define DEFAULT_BLUR_DOWNSCALE	    64
#define DEFAULT_ZOOM                1
#define DEFAULT_FRAMERATE	        30
#define DEFAULT_MIRRORED	        false
#define DEFAULT_STRETCHED           true
#define DEFAULT_VIGNETTE_ENABLED    true
#define DEFAULT_VIGNETTE_INTENSITY  1.0f
#define DEFAULT_VIGNETTE_RADIUS     0.99f
#define DEFAULT_VIGNETTE_SMOOTHNESS 0.4f
#define DEFAULT_AUTO_DETECTION      false
#define DEFAULT_AUTO_DETECTION_BRIGHTNESS_THRESHOLD 0.03f
#define DEFAULT_AUTO_DETECTION_BLACK_RATIO 0.60f

struct ResolutionSettings
{
    std::string name = "";
    UINT width = 0;
    UINT height = 0;
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
    UINT blurDownscale = 0;
    UINT blurPasses = 0;
    UINT blurSamples = 0;
    UINT frameRate = 0;
    bool mirrored = false;
    bool stretched = true;
    UINT zoom = 1;
    bool vignetteEnabled = true;
    float vignetteIntensity = 0.0f;
    float vignetteRadius = 0.0f;
    float vignetteSmoothness = 0.0f;
    Resolutions resolutions;
    bool useAutoDetection = false;
	float autoDetectionBrightnessThreshold = DEFAULT_AUTO_DETECTION_BRIGHTNESS_THRESHOLD;
	float autoDetectionBlackRatio = DEFAULT_AUTO_DETECTION_BLACK_RATIO;
};


bool ReadSettings(AppSettings& settings);
void SaveSettings(AppSettings& settings);
