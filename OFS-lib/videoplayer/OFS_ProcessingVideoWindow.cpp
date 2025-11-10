#include "OFS_ProcessingVideoWindow.h"

#include "OFS_GL.h"
#include "OFS_Localization.h"
#include "OFS_EventSystem.h"
#include "OFS_ImGui.h"
#include "OFS_Profiling.h"

#include "state/states/ProcessingVideoWindowState.h"

bool OFS_ProcessingVideoWindow::Init() noexcept
{
	stateHandle = OFS_ProjectState<ProcessingVideoWindowState>::Register(ProcessingVideoWindowState::StateName);

	EV::Queue().appendListener(SDL_MOUSEWHEEL,
		OFS_SDL_Event::HandleEvent(EVENT_SYSTEM_BIND(this, &OFS_ProcessingVideoWindow::mouseScroll)));

	EV::Queue().appendListener(ProcessingFrameReadyEvent::EventType,
		ProcessingFrameReadyEvent::HandleEvent(EVENT_SYSTEM_BIND(this, &OFS_ProcessingVideoWindow::updateProcessingFrame)));

	videoImageId = ImGui::GetIDWithSeed("processingVideoImage", 0, rand());

	// Create OpenGL texture for processing frames
	glGenTextures(1, &processingTexture);
	glBindTexture(GL_TEXTURE_2D, processingTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frameWidth, frameHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return true;
}

OFS_ProcessingVideoWindow::~OFS_ProcessingVideoWindow() noexcept
{
	if (processingTexture) {
		glDeleteTextures(1, &processingTexture);
		processingTexture = 0;
	}
}

void OFS_ProcessingVideoWindow::updateProcessingFrame(const ProcessingFrameReadyEvent* ev) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (!processingTexture) return;

	// Update texture size if changed
	if (ev->width != frameWidth || ev->height != frameHeight) {
		frameWidth = ev->width;
		frameHeight = ev->height;
		glBindTexture(GL_TEXTURE_2D, processingTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frameWidth, frameHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	}

	// Upload frame data to GPU
	glBindTexture(GL_TEXTURE_2D, processingTexture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameWidth, frameHeight, GL_RGBA, GL_UNSIGNED_BYTE, ev->frameData);
}

void OFS_ProcessingVideoWindow::mouseScroll(const OFS_SDL_Event* ev) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	auto& state = ProcessingVideoWindowState::State(stateHandle);
	if (state.lockedPosition) return;

	auto& scroll = ev->sdl.wheel;
	if (videoHovered) {
		auto mousePosInVid = ImGui::GetMousePos() - viewportPos - windowPos - state.videoPos;
		float zoomPointX = (mousePosInVid.x - (videoDrawSize.x/2.f)) / videoDrawSize.x;
		float zoomPointY = (mousePosInVid.y - (videoDrawSize.y/2.f)) / videoDrawSize.y;

		zoomPointX *= frameWidth;
		zoomPointY *= frameHeight;

		const float oldScale = state.zoomFactor;
		state.zoomFactor *= 1 + (ZoomMulti * scroll.y);
		state.zoomFactor = Util::Clamp(state.zoomFactor, 0.1f, 10.f);

		const float scaleChange = (state.zoomFactor - oldScale) * baseScaleFactor;
		const float offsetX = -(zoomPointX * scaleChange);
		const float offsetY = -(zoomPointY * scaleChange);

		state.prevTranslation.x += offsetX;
		state.prevTranslation.y += offsetY;

		if (!dragStarted) {
			state.currentTranslation = state.prevTranslation;
		}
	}
}

void OFS_ProcessingVideoWindow::DrawProcessingVideo(bool* open) noexcept
{
	OFS_PROFILE(__FUNCTION__);
	if (open != nullptr && !*open) return;

	ImGui::Begin("Processing Pipeline###PROCESSINGVIDEO", open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

	if (!processingTexture) {
		ImGui::TextWrapped("Processing video pipeline not initialized");
		ImGui::End();
		return;
	}

	viewportPos = ImGui::GetWindowViewport()->Pos;
	auto drawList = ImGui::GetWindowDrawList();
	auto& state = ProcessingVideoWindowState::State(stateHandle);

	// Calculate video display size
	ImVec2 availSize = ImGui::GetContentRegionAvail();
	float vidAspect = (float)frameWidth / (float)frameHeight;
	ImVec2 videoSize;

	if (availSize.x / availSize.y > vidAspect) {
		videoSize.y = availSize.y;
		videoSize.x = videoSize.y * vidAspect;
	} else {
		videoSize.x = availSize.x;
		videoSize.y = videoSize.x / vidAspect;
	}

	baseScaleFactor = videoSize.x / (float)frameWidth;
	videoSize.x *= state.zoomFactor;
	videoSize.y *= state.zoomFactor;

	// Handle drag
	windowPos = ImGui::GetWindowPos() - viewportPos;
	if (!state.lockedPosition && videoHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !dragStarted) {
		dragStarted = true;
	}
	else if (dragStarted && videoHovered) {
		state.currentTranslation = state.prevTranslation + ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
	}

	// Set cursor position with zoom and pan
	ImVec2 videoPos = state.videoPos + state.currentTranslation;
	ImGui::SetCursorPos(videoPos);

	// Draw the processing video texture
	ImVec2 uv0(0, 0);
	ImVec2 uv1(1, 1);
	OFS::ImageWithId(videoImageId, (void*)(intptr_t)processingTexture, videoSize, uv0, uv1);

	// Right-click menu
	if (ImGui::BeginPopupContextItem()) {
		ImGui::MenuItem(TR(LOCK), NULL, &state.lockedPosition);
		ImGui::EndPopup();
	}

	videoHovered = ImGui::IsItemHovered() && ImGui::IsWindowHovered();
	videoDrawSize = ImGui::GetItemRectSize();

	// Cancel drag
	if ((dragStarted && !videoHovered) || ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		dragStarted = false;
		state.prevTranslation = state.currentTranslation;
	}

	// Recenter on middle click
	if (videoHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
		ResetTranslationAndZoom();
	}

	// Display stats
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
	ImGui::Text("Processing Pipeline: %dx%d", frameWidth, frameHeight);
	ImGui::Text("Zoom: %.1f%%", state.zoomFactor * 100.f);

	// VR Processing Controls (using VrShader approach from main window)
	if (ImGui::CollapsingHeader("VR Processing Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::PushItemWidth(200);

		// Video Type Override
		const char* videoTypeItems[] = { "Auto Detect", "Force 2D", "Force VR" };
		int videoTypeIdx = static_cast<int>(state.videoType);
		if (ImGui::Combo("Video Type", &videoTypeIdx, videoTypeItems, 3)) {
			state.videoType = static_cast<ProcessingVideoType>(videoTypeIdx);
		}
		OFS::Tooltip("Override auto-detection of 2D vs VR video");

		// VR Layout Override
		const char* layoutItems[] = { "Auto Detect", "Force SBS", "Force Top/Bottom" };
		int layoutIdx = static_cast<int>(state.vrLayout);
		if (ImGui::Combo("VR Layout", &layoutIdx, layoutItems, 3)) {
			state.vrLayout = static_cast<ProcessingVRLayout>(layoutIdx);
		}
		OFS::Tooltip("Override auto-detection of VR layout (SBS or TB)");

		ImGui::Separator();

		// Eye Selection
		ImGui::Checkbox("Use Right Eye", &state.useRightEye);
		OFS::Tooltip("Select which eye to extract from SBS/TB layout");

		ImGui::Separator();

		// Pitch Control
		ImGui::Text("VR View Adjustment");
		if (ImGui::SliderFloat("Pitch", &state.vrPitch, -90.0f, 90.0f, "%.1f°")) {
			// Value updated
		}
		OFS::Tooltip("Adjust vertical viewing angle\n-90° = looking down, 0° = level, +90° = looking up");

		// Zoom Control
		if (ImGui::SliderFloat("Zoom", &state.vrZoom, 0.05f, 2.0f, "%.2f")) {
			// Value updated
		}
		OFS::Tooltip("VR zoom factor (lower values = more zoomed in)");

		ImGui::Separator();

		// Reset button
		if (ImGui::Button("Reset VR Settings")) {
			state.vrPitch = -21.0f;
			state.vrZoom = 0.2f;
			state.useRightEye = false;
			state.videoType = ProcessingVideoType::Auto;
			state.vrLayout = ProcessingVRLayout::Auto;
		}

		ImGui::PopItemWidth();
	}

	ImGui::End();
}

void OFS_ProcessingVideoWindow::ResetTranslationAndZoom() noexcept
{
	auto& state = ProcessingVideoWindowState::State(stateHandle);
	if (state.lockedPosition) return;
	state.zoomFactor = 1.f;
	state.prevTranslation = ImVec2(0.f, 0.f);
	state.currentTranslation = ImVec2(0.f, 0.f);
}
