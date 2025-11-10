#include "OFS_VRFormatDetector.h"
#include "OFS_Util.h"
#include <algorithm>
#include <cmath>

VRFormatInfo OFS_VRFormatDetector::DetectFormat(
    int width,
    int height,
    const std::string& filename
) noexcept
{
    VRFormatInfo info;

    // Check aspect ratio
    float aspect = GetAspectRatio(width, height);
    bool isSBS = IsSBSAspect(aspect);
    bool isTB = IsTBAspect(aspect);

    // Check resolution (VR typically >2048p)
    bool isHighRes = (width >= 2048 || height >= 2048);

    // Filename analysis
    bool hasVRKeyword = HasVRKeyword(filename);
    VRProjection filenameProjection = DetectProjectionFromFilename(filename);
    VRLayout filenameLayout = DetectLayoutFromFilename(filename);

    // Decision logic (similar to Python implementation)
    if ((isSBS || isTB) && isHighRes) {
        info.isVR = true;
        info.confidence = 0.9f;
    }
    else if (hasVRKeyword) {
        info.isVR = true;
        info.confidence = 0.7f;
    }
    else {
        info.isVR = false;
        info.confidence = 0.0f;
        return info;
    }

    // Determine layout
    if (filenameLayout != VRLayout::None) {
        info.layout = filenameLayout;
    }
    else if (isSBS) {
        info.layout = VRLayout::SideBySide;
    }
    else if (isTB) {
        info.layout = VRLayout::TopBottom;
    }
    else {
        info.layout = VRLayout::None; // Mono
    }

    // Determine projection
    if (filenameProjection != VRProjection::None) {
        info.projection = filenameProjection;
    }
    else {
        // Default to equirectangular 180° for VR content
        info.projection = VRProjection::Equirectangular180;
    }

    return info;
}

bool OFS_VRFormatDetector::IsLikelyVR(int width, int height) noexcept
{
    float aspect = GetAspectRatio(width, height);
    bool isHighRes = (width >= 2048 || height >= 2048);
    return (IsSBSAspect(aspect) || IsTBAspect(aspect)) && isHighRes;
}

float OFS_VRFormatDetector::GetAspectRatio(int width, int height) noexcept
{
    if (height == 0) return 0.0f;
    return static_cast<float>(width) / static_cast<float>(height);
}

bool OFS_VRFormatDetector::IsSBSAspect(float aspect) noexcept
{
    // Side-by-Side: ~2:1 ratio (1.8 to 2.2)
    return aspect >= 1.8f && aspect <= 2.2f;
}

bool OFS_VRFormatDetector::IsTBAspect(float aspect) noexcept
{
    // Top-Bottom: ~1:2 ratio (0.45 to 0.55)
    return aspect >= 0.45f && aspect <= 0.55f;
}

bool OFS_VRFormatDetector::HasVRKeyword(const std::string& filename) noexcept
{
    std::string upper = filename;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    const char* keywords[] = {
        "VR", "_180", "_360", "SBS", "_TB", "FISHEYE",
        "EQUIRECTANGULAR", "LR_", "OCULUS", "_3DH", "MKX200"
    };

    for (const char* kw : keywords) {
        if (upper.find(kw) != std::string::npos) {
            return true;
        }
    }

    return false;
}

VRProjection OFS_VRFormatDetector::DetectProjectionFromFilename(const std::string& filename) noexcept
{
    std::string upper = filename;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    // Check for fisheye
    if (upper.find("FISHEYE") != std::string::npos) {
        if (upper.find("200") != std::string::npos) {
            return VRProjection::Fisheye200;
        }
        return VRProjection::Fisheye190;
    }

    // Check for equirectangular
    if (upper.find("EQUIRECT") != std::string::npos || upper.find("360") != std::string::npos) {
        return VRProjection::Equirectangular360;
    }

    if (upper.find("180") != std::string::npos) {
        return VRProjection::Equirectangular180;
    }

    return VRProjection::None;
}

VRLayout OFS_VRFormatDetector::DetectLayoutFromFilename(const std::string& filename) noexcept
{
    std::string upper = filename;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    // Check for side-by-side
    if (upper.find("SBS") != std::string::npos ||
        upper.find("LR_") != std::string::npos ||
        upper.find("_LR") != std::string::npos) {
        return VRLayout::SideBySide;
    }

    // Check for top-bottom
    if (upper.find("_TB") != std::string::npos ||
        upper.find("TB_") != std::string::npos) {
        return VRLayout::TopBottom;
    }

    return VRLayout::None;
}
