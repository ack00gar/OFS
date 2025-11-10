#pragma once
#include "imgui.h"

#include "OFS_Reflection.h"
#include "OFS_BinarySerialization.h"
#include "OFS_Util.h"
#include "OFS_Localization.h"
#include "OFS_VideoplayerEvents.h"

#include <string>
#include <vector>

// Window to display downscaled processing frames from dual-pipeline
// Used for visualizing tracking overlays (grid, motion vectors, bounding boxes)
class OFS_ProcessingVideoWindow
{
public:
	~OFS_ProcessingVideoWindow() noexcept;
	uint32_t StateHandle() const noexcept { return stateHandle; }
private:
	ImGuiID videoImageId;
	ImVec2 videoDrawSize;
	ImVec2 viewportPos;
	ImVec2 windowPos;

	uint32_t stateHandle = 0xFFFF'FFFF;
	uint32_t processingTexture = 0;  // GL texture for displaying processing frames

	int frameWidth = 640;
	int frameHeight = 640;

	bool videoHovered = false;
	bool dragStarted = false;

	float baseScaleFactor = 1.f;
	static constexpr float ZoomMulti = 0.05f;

	void mouseScroll(const OFS_SDL_Event* ev) noexcept;
	void updateProcessingFrame(const ProcessingFrameReadyEvent* ev) noexcept;

public:
	static constexpr const char* WindowId = "###PROCESSINGVIDEO";
	bool Init() noexcept;
	void DrawProcessingVideo(bool* open) noexcept;
	void ResetTranslationAndZoom() noexcept;
};
