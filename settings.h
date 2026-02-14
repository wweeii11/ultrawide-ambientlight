#pragma once

#include <vector>
#include <string>

#include "windows.h"

#define DEFAULT_BLUR_SAMPLES        5
#define DEFAULT_BLUR_PASSES		    3
#define DEFAULT_BLUR_DOWNSCALE	    64
#define DEFAULT_ZOOM                1
#define DEFAULT_FRAMERATE	        30
#define DEFAULT_MIRRORED	        true
#define DEFAULT_STRETCHED           true
#define DEFAULT_STRETCH_FACTOR      2.0f
#define DEFAULT_VIGNETTE_ENABLED    true
#define DEFAULT_VIGNETTE_INTENSITY  1.0f
#define DEFAULT_VIGNETTE_RADIUS     0.99f
#define DEFAULT_VIGNETTE_SMOOTHNESS 0.4f
#define DEFAULT_AUTO_DETECTION      true
#define DEFAULT_AUTO_DETECTION_TIME 500
#define DEFAULT_AUTO_DETECTION_BRIGHTNESS_THRESHOLD 0.03f
#define DEFAULT_AUTO_DETECTION_BLACK_RATIO 0.60f
#define DEFAULT_AUTO_DETECTION_LIGHT_MASK true
#define DEFAULT_AUTO_DETECTION_SYMMETRIC_BARS false
#define DEFAULT_AUTO_DETECTION_RESERVED_AREA false
#define DEFAULT_AUTO_DETECTION_RESERVED_WIDTH 16
#define DEFAULT_AUTO_DETECTION_RESERVED_HEIGHT 9
#define DEFAULT_SHOW_IN_TASKBAR     true

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
    bool loaded = false;
    UINT gameWidth = 0;
    UINT gameHeight = 0;
    UINT blurDownscale = 0;
    UINT blurPasses = DEFAULT_BLUR_PASSES;
    UINT blurSamples = DEFAULT_BLUR_SAMPLES;
    UINT frameRate = DEFAULT_FRAMERATE;
    bool mirrored = DEFAULT_MIRRORED;
    bool stretched = DEFAULT_STRETCHED;
    float stretchFactor = DEFAULT_STRETCH_FACTOR;
    UINT zoom = DEFAULT_ZOOM;
    bool vignetteEnabled = DEFAULT_VIGNETTE_ENABLED;
    float vignetteIntensity = DEFAULT_VIGNETTE_INTENSITY;
    float vignetteRadius = DEFAULT_VIGNETTE_RADIUS;
    float vignetteSmoothness = DEFAULT_VIGNETTE_SMOOTHNESS;
    Resolutions resolutions;
    bool useAutoDetection = DEFAULT_AUTO_DETECTION;
    float autoDetectionBrightnessThreshold = DEFAULT_AUTO_DETECTION_BRIGHTNESS_THRESHOLD;
    float autoDetectionBlackRatio = DEFAULT_AUTO_DETECTION_BLACK_RATIO;
    int autoDetectionTime = DEFAULT_AUTO_DETECTION_TIME;
    bool autoDetectionLightMask = DEFAULT_AUTO_DETECTION_LIGHT_MASK;
    bool autoDetectionSymmetricBars = DEFAULT_AUTO_DETECTION_SYMMETRIC_BARS;
    bool autoDetectionReservedArea = DEFAULT_AUTO_DETECTION_RESERVED_AREA;
    UINT autoDetectionReservedWidth = DEFAULT_AUTO_DETECTION_RESERVED_WIDTH;
    UINT autoDetectionReservedHeight = DEFAULT_AUTO_DETECTION_RESERVED_HEIGHT;
    bool showInTaskbar = DEFAULT_SHOW_IN_TASKBAR;
};


bool ReadSettings(AppSettings& settings);
void SaveSettings(AppSettings& settings);
