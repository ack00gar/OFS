#pragma once
#include <cstdint>
#include <string>

#include "OFS_Event.h"

enum class VideoplayerType : uint8_t
{
	Main,
	Preview
};

class VideoLoadedEvent : public OFS_Event<VideoLoadedEvent>
{
	public:
	std::string videoPath;
	VideoplayerType playerType;

	VideoLoadedEvent(const char* path, VideoplayerType type) noexcept
		: videoPath(path), playerType(type) {}
	VideoLoadedEvent(const std::string& path, VideoplayerType type) noexcept
		: videoPath(path), playerType(type) {}	
};

class PlayPauseChangeEvent : public OFS_Event<PlayPauseChangeEvent>
{
	public:
	bool paused = false;
	VideoplayerType playerType;
	PlayPauseChangeEvent(bool pause, VideoplayerType type) noexcept
		: paused(pause), playerType(type) {}
};

class TimeChangeEvent : public OFS_Event<TimeChangeEvent>
{
	public:
	float time;
	VideoplayerType playerType;
	TimeChangeEvent(float time, VideoplayerType type) noexcept
		: playerType(type), time(time) {} 
};

class DurationChangeEvent : public OFS_Event<DurationChangeEvent>
{
	public:
	float duration;
	VideoplayerType playerType;
	DurationChangeEvent(float duration, VideoplayerType type) noexcept
		: playerType(type), duration(duration) {}
};

class PlaybackSpeedChangeEvent : public OFS_Event<PlaybackSpeedChangeEvent>
{
	public:
	float playbackSpeed;
	VideoplayerType playerType;
	PlaybackSpeedChangeEvent(float speed, VideoplayerType type) noexcept
		: playerType(type), playbackSpeed(speed) {}

};

// Event emitted with downscaled frames for AI tracking (YOLO, optical flow, etc.)
// Dual-pipeline architecture: display path remains full resolution, processing path is downscaled
class ProcessingFrameReadyEvent : public OFS_Event<ProcessingFrameReadyEvent>
{
	public:
	const uint8_t* frameData;  // Pointer to downscaled frame data (RGBA)
	int width;                  // Processing frame width (e.g., 640)
	int height;                 // Processing frame height (e.g., 640)
	double timeSeconds;         // Timestamp
	VideoplayerType playerType;
	int originalWidth;          // Original video width (for coordinate transformation)
	int originalHeight;         // Original video height

	ProcessingFrameReadyEvent(const uint8_t* data, int w, int h, double time,
	                          VideoplayerType type, int origW, int origH) noexcept
		: frameData(data), width(w), height(h), timeSeconds(time),
		  playerType(type), originalWidth(origW), originalHeight(origH) {}
};