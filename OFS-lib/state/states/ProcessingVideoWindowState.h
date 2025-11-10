#pragma once

#include "OFS_StateHandle.h"

enum class ProcessingVideoType : int32_t {
    Auto = 0,
    Force2D,
    ForceVR
};

enum class ProcessingVRLayout : int32_t {
    Auto = 0,
    ForceSBS,
    ForceTB
};

struct ProcessingVideoWindowState
{
    static constexpr auto StateName = "ProcessingVideoWindowState";

    ImVec2 currentTranslation = ImVec2(0.0f, 0.0f);
    ImVec2 videoPos = ImVec2(0.0f, 0.0f);
    ImVec2 prevTranslation = currentTranslation;

    float zoomFactor = 1.f;
    bool lockedPosition = false;

    // VR processing settings (using VrShader approach from main window)
    ProcessingVideoType videoType = ProcessingVideoType::Auto;
    ProcessingVRLayout vrLayout = ProcessingVRLayout::Auto;
    bool useRightEye = false;  // false = left eye, true = right eye
    float vrPitch = -21.0f;    // Adjustable pitch for processing pipeline
    float vrZoom = 0.2f;       // VR zoom factor (lower = more zoom)

	inline static ProcessingVideoWindowState& State(uint32_t stateHandle) noexcept {
		return OFS_ProjectState<ProcessingVideoWindowState>(stateHandle).Get();
	}
};

REFL_TYPE(ProcessingVideoWindowState)
    REFL_FIELD(currentTranslation)
    REFL_FIELD(videoPos)
    REFL_FIELD(prevTranslation)
    REFL_FIELD(zoomFactor)
    REFL_FIELD(lockedPosition)
    REFL_FIELD(videoType, serializeEnum{})
    REFL_FIELD(vrLayout, serializeEnum{})
    REFL_FIELD(useRightEye)
    REFL_FIELD(vrPitch)
    REFL_FIELD(vrZoom)
REFL_END
