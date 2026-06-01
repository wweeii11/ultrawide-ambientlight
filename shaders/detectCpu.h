#pragma once

inline bool isLineMostlyBlack(const float* data, UINT length, UINT stride, float blackThreshold, float blackRatio, float varianceThreshold)
{
    int darkPixelCount = 0;
    float sum = 0.0f;
    float sumSq = 0.0f;

    for (UINT i = 0; i < length; ++i)
    {
        float pixel = data[i * stride];
        if (pixel <= blackThreshold)
        {
            ++darkPixelCount;
            sum += pixel;
            sumSq += pixel * pixel;
        }
    }

    if (darkPixelCount == 0)
        return false;

    float darkRatio = (float)darkPixelCount / (float)length;
    if (darkRatio < blackRatio)
        return false;

    // Variance of the dark pixels only: Var = E[x^2] - E[x]^2
    float n = (float)darkPixelCount;
    float mean = sum / n;
    float variance = (sumSq / n) - (mean * mean);

    // True black (bars/chrome) has near-zero variance — all pixels clump around 0
    // Dark HDR content has measurable variance even if all pixels are below threshold
    return (variance <= varianceThreshold);
}

// The Core Center-Out Algorithm - Returns the size of the bar in pixels
UINT FindBarSizeCenterOut(const float* luma, UINT pitch, int width, int height,
    bool isVerticalScan, int startIdx, int step, int edgeIdx,
    float blackThreshold, float blackRatio, float blackVariance, int minBarSize)
{
    // Tracks the furthest line out that we are SURE belongs to the continuous content
    int lastKnownContentLine = startIdx;

    int lineLength = isVerticalScan ? width : height;
    UINT stride = isVerticalScan ? 1 : pitch;

    // Scan from the center out to the edge
    for (int i = startIdx; (step > 0) ? (i <= edgeIdx) : (i >= edgeIdx); i += step)
    {
        // 1. Calculate current remaining runway to the physical edge
        int linesRemainingToEdge = (step < 0) ? i : (edgeIdx - i);

        // 2. Calculate the current running content half-dimension
        int currentContentHalfSize = std::abs(lastKnownContentLine - startIdx);
        if (currentContentHalfSize < 4) { currentContentHalfSize = 4; }

        // 3. Derive our adaptive UI thickness threshold dynamically
        int adaptiveUiThicknessMax = static_cast<int>(currentContentHalfSize * 0.3f);

        // --- ENHANCED HARD EARLY BREAKS ---
        if (lastKnownContentLine == (i - step))
        {
            // Condition A: We are currently inside continuous content.
            // If the remaining space can't even hold our minimum bar requirement, stop.
            if (linesRemainingToEdge < minBarSize)
            {
                return 0;
            }
        }
        else
        {
            // Condition B: We are currently floating in a potential black bar area.
            // If the remaining space is smaller than our adaptive UI limit, any new content 
            // would automatically be classified as UI. We can lock the border here.
            if (linesRemainingToEdge <= adaptiveUiThicknessMax)
            {
                break; // Break the loop and calculate the bar size based on lastKnownContentLine
            }
        }
        // ----------------------------------

        const float* linePtr = isVerticalScan ? (luma + i * pitch) : (luma + i);

        if (!isLineMostlyBlack(linePtr, lineLength, stride, blackThreshold, blackRatio, blackVariance))
        {
            int blockStart = i;
            int blockLength = 0;

            // 1. Measure the continuous block of bright lines ahead of us
            while ((step > 0 ? (i <= edgeIdx) : (i >= edgeIdx)) &&
                !isLineMostlyBlack(isVerticalScan ? (luma + i * pitch) : (luma + i),
                    lineLength, stride, blackThreshold, blackRatio, blackVariance))
            {
                blockLength++;
                i += step;
            }

            // 2. Dynamically calculate the verified content size so far (one-sided from center)
            int currentContentHalfSize = std::abs(lastKnownContentLine - startIdx);

            // Safety fallback: if we are right at the center and size is 0, provide a tiny baseline 
            // floor so we don't multiply by zero on the first few iterations.
            if (currentContentHalfSize < 4) { currentContentHalfSize = 4; }

            int adaptiveGapMax = static_cast<int>(currentContentHalfSize * 0.05f);
            int blackGapLeftBehind = std::abs(blockStart - lastKnownContentLine);

            // 4. Proportional Evaluation
            if (blockLength > adaptiveUiThicknessMax || blackGapLeftBehind <= adaptiveGapMax)
            {
                // It's either a massive block of light (content after a dark scene)
                // OR it's a thin line sitting almost directly flush against the content frame.
                // Extend the content boundaries to include this block.
                lastKnownContentLine = i - step;
            }
            else
            {
                // It's a thin bright block isolated by a substantial black gap.
                // Structurally, this behaves exactly like a UI element. Ignore it.
            }

            // Adjust loop index back by one step because the while loop went one line too far
            i -= step;
        }
    }

    // Convert last known content line into the final black bar size
    int barSize = 0;
    if (step < 0)
    {
        barSize = lastKnownContentLine;
    }
    else
    {
        int maxEdge = isVerticalScan ? height : width;
        barSize = maxEdge - 1 - lastKnownContentLine;
    }

    return (barSize < minBarSize) ? 0 : static_cast<UINT>(barSize);
}

// Evaluates line state instantly via the pre-calculated binary flags
inline bool IsLineActive(const UINT* activeFlags, int index)
{
    return activeFlags[index] == 1;
}

UINT FindBarSizeCenterOutWithFlags(const UINT* activeFlags, int startIdx, int step, int edgeIdx)
{
    int minBarSize = 16;
    int lastKnownMovieLine = startIdx;

    for (int i = startIdx; (step > 0) ? (i <= edgeIdx) : (i >= edgeIdx); i += step)
    {
        int linesRemainingToEdge = (step < 0) ? i : (edgeIdx - i);
        int currentMovieHalfSize = std::abs(lastKnownMovieLine - startIdx);
        if (currentMovieHalfSize < 4) { currentMovieHalfSize = 4; }

        int adaptiveUiThicknessMax = static_cast<int>(currentMovieHalfSize * 0.3f);

        // Hard Early Breaks
        if (lastKnownMovieLine == (i - step)) {
            if (linesRemainingToEdge < minBarSize) return 0;
        }
        else {
            if (linesRemainingToEdge <= adaptiveUiThicknessMax) break;
        }

        bool lineIsActive = IsLineActive(activeFlags, i);

        if (lineIsActive)
        {
            int blockStart = i;
            int blockLength = 0;

            while ((step > 0 ? (i <= edgeIdx) : (i >= edgeIdx)) && IsLineActive(activeFlags, i))
            {
                blockLength++;
                i += step;
            }

            int adaptiveGapMax = static_cast<int>(currentMovieHalfSize * 0.05f);
            int blackGapLeftBehind = std::abs(blockStart - lastKnownMovieLine);

            if (blockLength > adaptiveUiThicknessMax || blackGapLeftBehind <= adaptiveGapMax) {
                lastKnownMovieLine = i - step;
            }

            i -= step;
        }
    }

    int barSize = (step < 0) ? lastKnownMovieLine : (edgeIdx - lastKnownMovieLine);
    return (barSize < minBarSize) ? 0 : static_cast<UINT>(barSize);
}

