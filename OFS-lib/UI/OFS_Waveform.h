#pragma once

#include <vector>
#include <string>
#include <memory>

#include "OFS_BinarySerialization.h"
#include "OFS_Shader.h"
#include "imgui.h"



// LOD level for efficient waveform rendering at different zoom levels
struct WaveformLODLevel
{
	std::vector<float> maxValues;  // Pre-computed max values for this LOD
	int samplesPerPixel;           // How many samples each value represents
};

// helper class to render audio waves
class OFS_Waveform
{
	bool generating = false;
	std::vector<float> samples;
	std::vector<WaveformLODLevel> lodLevels;  // LOD pyramid for fast rendering
public:

	inline bool BusyGenerating() noexcept { return generating; }
	bool GenerateAndLoadFlac(const std::string& ffmpegPath, const std::string& videoPath, const std::string& output) noexcept;
	bool LoadFlac(const std::string& path) noexcept;

	inline void Clear() noexcept {
		samples.clear();
		lodLevels.clear();
	}

	inline void SetSamples(std::vector<float>&& samples) noexcept
	{
		this->samples = std::move(samples);
		BuildLODPyramid();
	}

	inline const std::vector<float>& Samples() const noexcept { return samples; }

	inline size_t SampleCount() const noexcept {
		return samples.size();
	}

	// LOD pyramid methods
	void BuildLODPyramid() noexcept;
	const WaveformLODLevel* GetLODForSamplesPerPixel(int samplesPerPixel) const noexcept;
};

struct OFS_WaveformLOD
{
	std::vector<float> WaveformLineBuffer;
	std::unique_ptr<WaveformShader> WaveShader;
	ImColor WaveformColor = IM_COL32(227, 66, 52, 255);
	uint32_t WaveformTex = 0;
	float samplingOffset = 0.f;

	float lastCanvasX = 0.f;
	float lastVisibleDuration = 0.f;
	
	int32_t lastMultiple = 0.f;
	OFS_Waveform data;

	void Init() noexcept;
	void Update(const class OverlayDrawingCtx& ctx) noexcept;
	void Upload() noexcept;
};