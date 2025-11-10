#pragma once

#include <cstdint>
#include <string>

enum class VRProjection {
    None,           // 2D video
    Equirectangular180,
    Equirectangular360,
    Fisheye190,
    Fisheye200
};

enum class VRLayout {
    None,          // 2D or mono
    SideBySide,    // Left/Right
    TopBottom      // Top/Bottom
};

struct VRFormatInfo {
    bool isVR = false;
    VRProjection projection = VRProjection::None;
    VRLayout layout = VRLayout::None;

    // Detection confidence (0.0-1.0)
    float confidence = 0.0f;

    // Auto-detected or user-forced
    bool userForced = false;
};

class OFS_VRFormatDetector {
public:
    // Detect VR format from video properties
    static VRFormatInfo DetectFormat(
        int width,
        int height,
        const std::string& filename
    ) noexcept;

    // Check if resolution/aspect suggests VR
    static bool IsLikelyVR(int width, int height) noexcept;

    // Get aspect ratio
    static float GetAspectRatio(int width, int height) noexcept;

private:
    // Aspect ratio checks
    static bool IsSBSAspect(float aspect) noexcept;
    static bool IsTBAspect(float aspect) noexcept;

    // Filename heuristics
    static bool HasVRKeyword(const std::string& filename) noexcept;
    static VRProjection DetectProjectionFromFilename(const std::string& filename) noexcept;
    static VRLayout DetectLayoutFromFilename(const std::string& filename) noexcept;
};
