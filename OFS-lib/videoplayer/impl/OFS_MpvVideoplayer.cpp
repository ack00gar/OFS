#include "OFS_Videoplayer.h"
#include "OFS_Util.h"

#include "OFS_EventSystem.h"
#include "OFS_VideoplayerEvents.h"

#define OFS_MPV_LOADER_MACROS
#include "OFS_MpvLoader.h"

#include "OFS_Localization.h"
#include "OFS_GL.h"
#include "OFS_Shader.h"
#include "videoplayer/OFS_VRFormatDetector.h"
#include "state/states/ProcessingVideoWindowState.h"
#include "state/OFS_StateManager.h"

#include <sstream>

#include "SDL_timer.h"
#include "SDL_atomic.h"

enum MpvPropertyGet : uint64_t {
    MpvDuration,
    MpvPosition,
    MpvTotalFrames,
    MpvSpeed,
    MpvVideoWidth,
    MpvVideoHeight,
    MpvPauseState,
    MpvFilePath,
    MpvHwDecoder,
    MpvFramesPerSecond,
};

struct MpvDataCache {
    double duration = 1.0;
    double percentPos = 0.0;
    double currentSpeed = 1.0;
    double fps = 30.0;
    double averageFrameTime = 1.0/fps;
    
    double abLoopA = 0;
    double abLoopB = 0;

    int64_t totalNumFrames = 0;
    int64_t paused = false;
    int64_t videoWidth = 0;
    int64_t videoHeight = 0;

    float currentVolume = .5f;

    bool videoLoaded = false;
    std::string filePath = "";
};

struct MpvPlayerContext
{
    mpv_handle* mpv = nullptr;
    mpv_render_context* mpvGL = nullptr;
    uint32_t framebuffer = 0;
    MpvDataCache data = MpvDataCache();

    std::array<char, 32> tmpBuf;
    SDL_atomic_t renderUpdate = {0};
    SDL_atomic_t hasEvents = {0};

    uint32_t* frameTexture = nullptr;
    float* logicalPosition = nullptr;

    uint64_t smoothTimer = 0;
    VideoplayerType playerType;

    // Generic processing path for AI tracking (YOLO, optical flow, etc.)
    // Renders downscaled frames for efficient CPU processing
    static constexpr int PROCESSING_SIZE = 640;
    uint32_t processingFramebuffer = 0;
    uint32_t processingTexture = 0;
    uint32_t processingPBO[2] = {0, 0};  // Double-buffered PBO for async readback
    int processingPBOIndex = 0;
    bool trackingActive = false;  // Set by tracking systems

    // VR unwarp pipeline resources
    uint32_t fullResFramebuffer = 0;     // FBO for full-resolution VR render (before crop)
    uint32_t fullResTexture = 0;
    uint32_t croppedFramebuffer = 0;     // FBO for cropped VR panel (SBS/TB → single eye)
    uint32_t croppedTexture = 0;
    uint32_t unwarpedFramebuffer = 0;    // FBO for unwarped output
    uint32_t unwarpedTexture = 0;
    uint32_t quadVAO = 0;                // Full-screen quad for shader rendering
    uint32_t quadVBO = 0;

    // VR detection and settings
    VRFormatInfo vrFormat;
    bool vrDetectionDone = false;

    // VR unwarp shaders
    VRCropShader* cropShader = nullptr;
    VrShader* vrShader = nullptr;  // Use the proven VR shader from main window

    // State handle for VR settings
    uint32_t vrStateHandle = 0;
};

#define CTX static_cast<MpvPlayerContext*>(ctx)

static void OnMpvEvents(void* ctx) noexcept
{
    SDL_AtomicIncRef(&CTX->hasEvents);
}

static void OnMpvRenderUpdate(void* ctx) noexcept
{
    SDL_AtomicIncRef(&CTX->renderUpdate);
}

inline static void notifyVideoLoaded(MpvPlayerContext* ctx) noexcept
{
    EV::Enqueue<VideoLoadedEvent>(CTX->data.filePath, CTX->playerType);
}

inline static void notifyPaused(MpvPlayerContext* ctx) noexcept
{
    EV::Enqueue<PlayPauseChangeEvent>(CTX->data.paused, CTX->playerType);
}

inline static void notifyTime(MpvPlayerContext* ctx) noexcept
{
    EV::Enqueue<TimeChangeEvent>((float)(CTX->data.duration * CTX->data.percentPos), CTX->playerType);
}

inline static void notifyDuration(MpvPlayerContext* ctx) noexcept
{
    EV::Enqueue<DurationChangeEvent>((float)CTX->data.duration, CTX->playerType);   
}

inline static void notifyPlaybackSpeed(MpvPlayerContext* ctx) noexcept
{
    EV::Enqueue<PlaybackSpeedChangeEvent>((float)CTX->data.currentSpeed, CTX->playerType);
}

inline static void cleanupOpenGLResources(MpvPlayerContext* ctx) noexcept
{
	// Clean up processing pipeline resources
	if (ctx->processingFramebuffer) {
		glDeleteFramebuffers(1, &ctx->processingFramebuffer);
		ctx->processingFramebuffer = 0;
	}
	if (ctx->processingTexture) {
		glDeleteTextures(1, &ctx->processingTexture);
		ctx->processingTexture = 0;
	}
	if (ctx->processingPBO[0] || ctx->processingPBO[1]) {
		glDeleteBuffers(2, ctx->processingPBO);
		ctx->processingPBO[0] = 0;
		ctx->processingPBO[1] = 0;
	}

	// Clean up VR pipeline FBOs
	if (ctx->fullResFramebuffer) {
		glDeleteFramebuffers(1, &ctx->fullResFramebuffer);
		ctx->fullResFramebuffer = 0;
	}
	if (ctx->fullResTexture) {
		glDeleteTextures(1, &ctx->fullResTexture);
		ctx->fullResTexture = 0;
	}
	if (ctx->croppedFramebuffer) {
		glDeleteFramebuffers(1, &ctx->croppedFramebuffer);
		ctx->croppedFramebuffer = 0;
	}
	if (ctx->croppedTexture) {
		glDeleteTextures(1, &ctx->croppedTexture);
		ctx->croppedTexture = 0;
	}
	if (ctx->unwarpedFramebuffer) {
		glDeleteFramebuffers(1, &ctx->unwarpedFramebuffer);
		ctx->unwarpedFramebuffer = 0;
	}
	if (ctx->unwarpedTexture) {
		glDeleteTextures(1, &ctx->unwarpedTexture);
		ctx->unwarpedTexture = 0;
	}

	// Clean up quad VAO/VBO
	if (ctx->quadVAO) {
		glDeleteVertexArrays(1, &ctx->quadVAO);
		ctx->quadVAO = 0;
	}
	if (ctx->quadVBO) {
		glDeleteBuffers(1, &ctx->quadVBO);
		ctx->quadVBO = 0;
	}

	// Clean up shaders
	if (ctx->cropShader) {
		delete ctx->cropShader;
		ctx->cropShader = nullptr;
	}
	if (ctx->vrShader) {
		delete ctx->vrShader;
		ctx->vrShader = nullptr;
	}

	// Clean up main framebuffer
	if (ctx->framebuffer) {
		glDeleteFramebuffers(1, &ctx->framebuffer);
		ctx->framebuffer = 0;
	}
}

inline static void updateProcessingFBO(MpvPlayerContext* ctx) noexcept
{
	// Create processing FBO for AI tracking (YOLO, optical flow, etc.)
	if (!ctx->processingFramebuffer) {
		// Create framebuffer
		glGenFramebuffers(1, &ctx->processingFramebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, ctx->processingFramebuffer);

		// Create texture
		glGenTextures(1, &ctx->processingTexture);
		glBindTexture(GL_TEXTURE_2D, ctx->processingTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, OFS_InternalTexFormat,
		            MpvPlayerContext::PROCESSING_SIZE, MpvPlayerContext::PROCESSING_SIZE,
		            0, OFS_TexFormat, GL_UNSIGNED_BYTE, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Attach to framebuffer
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		                      GL_TEXTURE_2D, ctx->processingTexture, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			LOG_ERROR("Failed to create processing FBO for AI tracking!");
		}

		// Create double-buffered PBOs for async readback
		glGenBuffers(2, ctx->processingPBO);
		int pboSize = MpvPlayerContext::PROCESSING_SIZE * MpvPlayerContext::PROCESSING_SIZE * 4;
		for (int i = 0; i < 2; ++i) {
			glBindBuffer(GL_PIXEL_PACK_BUFFER, ctx->processingPBO[i]);
			glBufferData(GL_PIXEL_PACK_BUFFER, pboSize, 0, GL_STREAM_READ);
		}
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		LOG_INFO("Processing FBO created for AI tracking (640x640 with async PBO readback)");
	}

	// Create VR pipeline FBOs (crop + unwarp)
	if (!ctx->fullResFramebuffer) {
		// Full-resolution FBO (for rendering VR video before crop)
		// This will be resized dynamically to match video dimensions
		glGenFramebuffers(1, &ctx->fullResFramebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, ctx->fullResFramebuffer);

		glGenTextures(1, &ctx->fullResTexture);
		glBindTexture(GL_TEXTURE_2D, ctx->fullResTexture);
		// Initial size - will be resized when video dimensions are known
		glTexImage2D(GL_TEXTURE_2D, 0, OFS_InternalTexFormat,
		            1920, 1080,  // Placeholder size
		            0, OFS_TexFormat, GL_UNSIGNED_BYTE, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		                      GL_TEXTURE_2D, ctx->fullResTexture, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			LOG_ERROR("Failed to create full-resolution FBO for VR processing!");
		}

		LOG_INFO("Full-resolution VR FBO created");
	}

	if (!ctx->croppedFramebuffer) {
		// Cropped FBO (for VR panel extraction)
		glGenFramebuffers(1, &ctx->croppedFramebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, ctx->croppedFramebuffer);

		glGenTextures(1, &ctx->croppedTexture);
		glBindTexture(GL_TEXTURE_2D, ctx->croppedTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, OFS_InternalTexFormat,
		            MpvPlayerContext::PROCESSING_SIZE, MpvPlayerContext::PROCESSING_SIZE,
		            0, OFS_TexFormat, GL_UNSIGNED_BYTE, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		                      GL_TEXTURE_2D, ctx->croppedTexture, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			LOG_ERROR("Failed to create cropped FBO for VR processing!");
		}

		// Unwarped FBO (for unwarp shader output)
		glGenFramebuffers(1, &ctx->unwarpedFramebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, ctx->unwarpedFramebuffer);

		glGenTextures(1, &ctx->unwarpedTexture);
		glBindTexture(GL_TEXTURE_2D, ctx->unwarpedTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, OFS_InternalTexFormat,
		            MpvPlayerContext::PROCESSING_SIZE, MpvPlayerContext::PROCESSING_SIZE,
		            0, OFS_TexFormat, GL_UNSIGNED_BYTE, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		                      GL_TEXTURE_2D, ctx->unwarpedTexture, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			LOG_ERROR("Failed to create unwarped FBO for VR processing!");
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		LOG_INFO("VR pipeline FBOs created (crop + unwarp)");
	}

	// Create full-screen quad for shader rendering
	if (!ctx->quadVAO) {
		float quadVertices[] = {
			// positions   // texCoords
			-1.0f,  1.0f,  0.0f, 1.0f,
			-1.0f, -1.0f,  0.0f, 0.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,

			-1.0f,  1.0f,  0.0f, 1.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,
			 1.0f,  1.0f,  1.0f, 1.0f
		};

		glGenVertexArrays(1, &ctx->quadVAO);
		glGenBuffers(1, &ctx->quadVBO);
		glBindVertexArray(ctx->quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, ctx->quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
		glBindVertexArray(0);

		LOG_INFO("Full-screen quad VAO/VBO created for VR shaders");
	}

	// Initialize VR shaders (lazily, only when needed)
	if (!ctx->cropShader) {
		ctx->cropShader = new VRCropShader();
		LOG_INFO("VRCropShader initialized");
	}
	if (!ctx->vrShader) {
		ctx->vrShader = new VrShader();
		LOG_INFO("VrShader initialized for processing pipeline");
	}
}

inline static void updateRenderTexture(MpvPlayerContext* ctx) noexcept
{
    if (!ctx->framebuffer) {
		glGenFramebuffers(1, &ctx->framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, ctx->framebuffer);

		glGenTextures(1, ctx->frameTexture);
		glBindTexture(GL_TEXTURE_2D, *ctx->frameTexture);
		
        int initialWidth = ctx->data.videoWidth > 0 ? ctx->data.videoWidth : 1920;
		int initialHeight = ctx->data.videoHeight > 0 ? ctx->data.videoHeight : 1080;
		glTexImage2D(GL_TEXTURE_2D, 0, OFS_InternalTexFormat, initialWidth, initialHeight, 0, OFS_TexFormat, GL_UNSIGNED_BYTE, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Set "renderedTexture" as our colour attachement #0
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *ctx->frameTexture, 0);

		// Set the list of draw buffers.
		GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, DrawBuffers); 

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			LOG_ERROR("Failed to create framebuffer for video!");
		}
	}
	else if(ctx->data.videoHeight > 0 && ctx->data.videoWidth > 0) {
		// update size of render texture based on video resolution
		glBindTexture(GL_TEXTURE_2D, *ctx->frameTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, OFS_InternalTexFormat, ctx->data.videoWidth, ctx->data.videoHeight, 0, OFS_TexFormat, GL_UNSIGNED_BYTE, 0);
	}

	// Also create processing FBO for AI tracking
	updateProcessingFBO(ctx);
}

inline static void showText(MpvPlayerContext* ctx, const char* text) noexcept
{
    const char* cmd[] = { "show_text", text, NULL };
    mpv_command_async(ctx->mpv, 0, cmd);
}

OFS_Videoplayer::~OFS_Videoplayer() noexcept
{
    // Clean up OpenGL resources before destroying mpv context
    cleanupOpenGLResources(CTX);

    mpv_render_context_free(CTX->mpvGL);
	mpv_destroy(CTX->mpv);
    delete CTX;
    ctx = nullptr;
}

OFS_Videoplayer::OFS_Videoplayer(VideoplayerType playerType) noexcept
{
    this->playerType = playerType;
    ctx = new MpvPlayerContext();
    CTX->playerType = playerType;
    CTX->frameTexture = &this->frameTexture;
    CTX->logicalPosition = &this->logicalPosition;
}

bool OFS_Videoplayer::Init(bool hwAccel) noexcept
{
    CTX->mpv = mpv_create();
    if(!CTX->mpv) {
        return false;
    }
    auto confPath = Util::Prefpath();

    int error = 0;
    error = mpv_set_option_string(CTX->mpv, "config", "yes");
    if(error != 0) {
        LOG_WARN("Failed to set mpv: config=yes");
    }
    error = mpv_set_option_string(CTX->mpv, "config-dir", confPath.c_str());
    if(error != 0) {
        LOGF_WARN("Failed to set mpv: config-dir=%s", confPath.c_str());
    }

    if(mpv_initialize(CTX->mpv) != 0) {
        return false;
    }

    error = mpv_set_property_string(CTX->mpv, "loop-file", "inf");
    if(error != 0) {
        LOG_WARN("Failed to set mpv: loop-file=inf");
    }

    if(hwAccel) {
        error = mpv_set_property_string(CTX->mpv, "profile", "gpu-hq");
        if(error != 0) {
            LOG_WARN("Failed to set mpv: profile=gpu-hq");
        }
        error = mpv_set_property_string(CTX->mpv, "hwdec", "auto-safe");
        if(error != 0) {
            LOG_WARN("Failed to set mpv: hwdec=auto-safe");
        }
    }
    else {
        error = mpv_set_property_string(CTX->mpv, "hwdec", "no");
        if(error != 0) {
            LOG_WARN("Failed to set mpv: hwdec=no");
        }
    }

    // Set vo=libmpv explicitly for embedded contexts
    // Required for mpv 0.38+ on all platforms (see mpv API v2.3 changes)
    // Previously only needed on Apple Silicon, now needed everywhere
    error = mpv_set_option_string(CTX->mpv, "vo", "libmpv");
    if(error != 0) {
        LOG_WARN("Failed to set mpv: vo=libmpv");
    }

#if defined(__APPLE__) && defined(__arm64__)
    // Force software decoding and Cocoa context on M1/M2 Macs
    hwAccel = false;
    error = mpv_set_option_string(CTX->mpv, "gpu-context", "cocoa");
    if(error != 0) {
        LOG_WARN("Failed to set mpv: gpu-context=cocoa");
    }
#endif

#ifndef NDEBUG
    mpv_request_log_messages(CTX->mpv, "debug");
#else
    mpv_request_log_messages(CTX->mpv, "info");
#endif

    mpv_opengl_init_params init_params = {0};
	init_params.get_proc_address = [](void* mpvContext, const char* fnName) noexcept -> void*
    {
        return SDL_GL_GetProcAddress(fnName);
    };
    
    uint32_t enable = 1;
	mpv_render_param renderParams[] = {
		mpv_render_param{MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL},
		mpv_render_param{MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &init_params},
		mpv_render_param{MPV_RENDER_PARAM_ADVANCED_CONTROL, &enable },
		mpv_render_param{}
	};

    if (mpv_render_context_create(&CTX->mpvGL, CTX->mpv, renderParams) < 0) {
		LOG_ERROR("Failed to initialize mpv GL context");
		return false;
	}

    mpv_set_wakeup_callback(CTX->mpv, OnMpvEvents, ctx);
    mpv_render_context_set_update_callback(CTX->mpvGL, OnMpvRenderUpdate, ctx);

	mpv_observe_property(CTX->mpv, MpvVideoHeight, "height", MPV_FORMAT_INT64);
	mpv_observe_property(CTX->mpv, MpvVideoWidth, "width", MPV_FORMAT_INT64);
	mpv_observe_property(CTX->mpv, MpvDuration, "duration", MPV_FORMAT_DOUBLE);
	mpv_observe_property(CTX->mpv, MpvPosition, "percent-pos", MPV_FORMAT_DOUBLE);
	mpv_observe_property(CTX->mpv, MpvTotalFrames, "estimated-frame-count", MPV_FORMAT_INT64);
	mpv_observe_property(CTX->mpv, MpvSpeed, "speed", MPV_FORMAT_DOUBLE);
	mpv_observe_property(CTX->mpv, MpvPauseState, "pause", MPV_FORMAT_FLAG);
	mpv_observe_property(CTX->mpv, MpvFilePath, "path", MPV_FORMAT_STRING);
	mpv_observe_property(CTX->mpv, MpvHwDecoder, "hwdec-current", MPV_FORMAT_STRING);
	mpv_observe_property(CTX->mpv, MpvFramesPerSecond, "estimated-vf-fps", MPV_FORMAT_DOUBLE);

	// Register or get state handle for VR settings
	CTX->vrStateHandle = OFS_ProjectState<ProcessingVideoWindowState>::Register(ProcessingVideoWindowState::StateName);

    return true;
}

inline static void ProcessEvents(MpvPlayerContext* ctx) noexcept
{
    for(;;) {
		mpv_event* mp_event = mpv_wait_event(ctx->mpv, 0.);
		if (mp_event->event_id == MPV_EVENT_NONE)
			break;
			
		switch (mp_event->event_id) {
            case MPV_EVENT_LOG_MESSAGE:
            {
                mpv_event_log_message* msg = (mpv_event_log_message*)mp_event->data;
                char MpvLogPrefix[48];
                int len = stbsp_snprintf(MpvLogPrefix, sizeof(MpvLogPrefix), "[%s][MPV] (%s): ", msg->level, msg->prefix);
                FUN_ASSERT(len <= sizeof(MpvLogPrefix), "buffer to small");
                OFS_FileLogger::LogToFileR(MpvLogPrefix, msg->text);
                continue;
            }
            case MPV_EVENT_COMMAND_REPLY:
            {
                // attach user_data to command
                // and handle it here when it finishes
                continue;
            }
            case MPV_EVENT_FILE_LOADED:
            {
                ctx->data.videoLoaded = true;

                // Run VR detection when file is loaded (dimensions are now available)
                if (!ctx->vrDetectionDone && ctx->data.videoWidth > 0 && ctx->data.videoHeight > 0) {
                    LOGF_DEBUG("Running VR detection on file load: %dx%d, path=%s",
                              ctx->data.videoWidth, ctx->data.videoHeight, ctx->data.filePath.c_str());
                    ctx->vrFormat = OFS_VRFormatDetector::DetectFormat(
                        ctx->data.videoWidth,
                        ctx->data.videoHeight,
                        ctx->data.filePath
                    );
                    ctx->vrDetectionDone = true;

                    if (ctx->vrFormat.isVR) {
                        LOGF_INFO("VR video detected: %s layout, %s projection, confidence: %.2f",
                                 ctx->vrFormat.layout == VRLayout::SideBySide ? "SBS" :
                                 ctx->vrFormat.layout == VRLayout::TopBottom ? "TB" : "Mono",
                                 ctx->vrFormat.projection == VRProjection::Equirectangular180 ? "Equirect180" :
                                 ctx->vrFormat.projection == VRProjection::Equirectangular360 ? "Equirect360" :
                                 ctx->vrFormat.projection == VRProjection::Fisheye190 ? "Fisheye190" :
                                 ctx->vrFormat.projection == VRProjection::Fisheye200 ? "Fisheye200" : "None",
                                 ctx->vrFormat.confidence);
                    } else {
                        LOG_INFO("2D video detected");
                    }
                }
                continue;
            }
            case MPV_EVENT_PROPERTY_CHANGE:
            {
                mpv_event_property* prop = (mpv_event_property*)mp_event->data;
                LOGF_DEBUG("Property change: name=%s, userdata=%lld, format=%d, data=%p",
                          prop->name, mp_event->reply_userdata, prop->format, prop->data);
                if (prop->data == nullptr) break;
                switch (mp_event->reply_userdata) {
                    case MpvHwDecoder:
                    {
                        LOGF_INFO("Active hardware decoder: %s", *(char**)prop->data);
                        break;
                    }
                    case MpvVideoWidth:
                    {
                        ctx->data.videoWidth = *(int64_t*)prop->data;
                        if (ctx->data.videoHeight > 0.f) {
                            updateRenderTexture(ctx);
                            ctx->data.videoLoaded = true;
                        }
                        break;
                    }
                    case MpvVideoHeight:
                    {
                        ctx->data.videoHeight = *(int64_t*)prop->data;
                        LOGF_DEBUG("MpvVideoHeight property: width=%d, height=%d, vrDetectionDone=%d",
                                  ctx->data.videoWidth, ctx->data.videoHeight, ctx->vrDetectionDone);
                        if (ctx->data.videoWidth > 0) {
                            updateRenderTexture(ctx);
                            ctx->data.videoLoaded = true;

                            // Detect VR format when both width and height are known
                            if (!ctx->vrDetectionDone) {
                                LOGF_DEBUG("Running VR detection for: %s", ctx->data.filePath.c_str());
                                ctx->vrFormat = OFS_VRFormatDetector::DetectFormat(
                                    ctx->data.videoWidth,
                                    ctx->data.videoHeight,
                                    ctx->data.filePath
                                );
                                ctx->vrDetectionDone = true;

                                if (ctx->vrFormat.isVR) {
                                    LOGF_INFO("VR video detected: %s layout, %s projection, confidence: %.2f",
                                             ctx->vrFormat.layout == VRLayout::SideBySide ? "SBS" :
                                             ctx->vrFormat.layout == VRLayout::TopBottom ? "TB" : "Mono",
                                             ctx->vrFormat.projection == VRProjection::Equirectangular180 ? "Equirect180" :
                                             ctx->vrFormat.projection == VRProjection::Equirectangular360 ? "Equirect360" :
                                             ctx->vrFormat.projection == VRProjection::Fisheye190 ? "Fisheye190" :
                                             ctx->vrFormat.projection == VRProjection::Fisheye200 ? "Fisheye200" : "None",
                                             ctx->vrFormat.confidence);
                                } else {
                                    LOG_INFO("2D video detected");
                                }
                            }
                        }
                        break;
                    }
                    case MpvFramesPerSecond:
                        ctx->data.fps = *(double*)prop->data;
                        ctx->data.averageFrameTime = (1.0 / ctx->data.fps);
                        break;
                    case MpvDuration:
                        ctx->data.duration = *(double*)prop->data;
                        notifyDuration(ctx);
                        break;
                    case MpvTotalFrames:
                        ctx->data.totalNumFrames = *(int64_t*)prop->data;
                        break;
                    case MpvPosition:
                    {
                        auto newPercentPos = (*(double*)prop->data) / 100.0;
                        ctx->data.percentPos = newPercentPos;
                        ctx->smoothTimer = SDL_GetTicks64();
                        if(!ctx->data.paused) {
                            *ctx->logicalPosition = newPercentPos;
                        }
                        notifyTime(ctx);
                        break;
                    }
                    case MpvSpeed:
                        ctx->data.currentSpeed = *(double*)prop->data;
                        notifyPlaybackSpeed(ctx);
                        break;
                    case MpvPauseState:
                    {
                        bool paused = *(int64_t*)prop->data;
                        if (paused) {
                            float timeSinceLastUpdate = (SDL_GetTicks64() - CTX->smoothTimer) / 1000.f;
                            float positionOffset = (timeSinceLastUpdate * CTX->data.currentSpeed) / CTX->data.duration;
                            *ctx->logicalPosition += positionOffset;
                        }
                        ctx->smoothTimer = SDL_GetTicks64();
                        ctx->data.paused = paused;
                        notifyPaused(ctx);
                        break;
                    }
                    case MpvFilePath:
                        ctx->data.filePath = *((const char**)(prop->data));
                        notifyVideoLoaded(ctx);
                        break;
                }
                continue;
            }
		}
	}
}

inline static void RenderFrameToTexture(MpvPlayerContext* ctx) noexcept
{
	// Render once to main display FBO (full resolution)
	mpv_opengl_fbo mainFbo = {0};
	mainFbo.fbo = ctx->framebuffer;
	mainFbo.w = ctx->data.videoWidth;
	mainFbo.h = ctx->data.videoHeight;
	mainFbo.internal_format = OFS_InternalTexFormat;

	uint32_t disable = 0;
	mpv_render_param mainParams[] = {
		{MPV_RENDER_PARAM_OPENGL_FBO, &mainFbo},
		{MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &disable},
		mpv_render_param{}
	};
	mpv_render_context_render(ctx->mpvGL, mainParams);

	// Path 2: PROCESSING PIPELINE (downsample from main texture for AI tracking)
	// Only when tracking is active to avoid overhead
	if (ctx->trackingActive && ctx->processingFramebuffer && *ctx->frameTexture) {
		LOGF_INFO("Processing pipeline active: framebuffer=%u, texture=%u, videoSize=%dx%d",
		          ctx->processingFramebuffer, *ctx->frameTexture, ctx->data.videoWidth, ctx->data.videoHeight);

		// Run VR detection if not done yet and dimensions are available
		if (!ctx->vrDetectionDone && ctx->data.videoWidth > 0 && ctx->data.videoHeight > 0) {
			ctx->vrFormat = OFS_VRFormatDetector::DetectFormat(
				ctx->data.videoWidth,
				ctx->data.videoHeight,
				ctx->data.filePath
			);
			ctx->vrDetectionDone = true;

			if (ctx->vrFormat.isVR) {
				LOGF_INFO("VR video detected: %s layout, %s projection",
				         ctx->vrFormat.layout == VRLayout::SideBySide ? "SBS" :
				         ctx->vrFormat.layout == VRLayout::TopBottom ? "TB" : "Mono",
				         ctx->vrFormat.projection == VRProjection::Equirectangular180 ? "Equirect180" :
				         ctx->vrFormat.projection == VRProjection::Equirectangular360 ? "Equirect360" :
				         ctx->vrFormat.projection == VRProjection::Fisheye190 ? "Fisheye190" :
				         ctx->vrFormat.projection == VRProjection::Fisheye200 ? "Fisheye200" : "None");
			} else {
				LOG_INFO("2D video detected");
			}
		}

		// Downsample main texture to 640x640 processing texture
		// Use glBlitFramebuffer for efficient GPU-side downscale
		glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx->framebuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ctx->processingFramebuffer);
		glBlitFramebuffer(
			0, 0, ctx->data.videoWidth, ctx->data.videoHeight,
			0, 0, MpvPlayerContext::PROCESSING_SIZE, MpvPlayerContext::PROCESSING_SIZE,
			GL_COLOR_BUFFER_BIT, GL_LINEAR
		);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Check for GL errors (only log if error occurs)
		GLenum err = glGetError();
		if (err != GL_NO_ERROR) {
			LOGF_ERROR("OpenGL error after blit: 0x%x", err);
		}

		// GPU-based VR processing pipeline using VrShader (same as main window)
		// Pipeline: crop (SBS/TB → single eye) → VrShader unwarp → readback

		// Get VR settings from UI state
		auto& vrState = ProcessingVideoWindowState::State(ctx->vrStateHandle);

		// Apply UI overrides to VR format
		VRFormatInfo activeFormat = ctx->vrFormat;  // Start with auto-detected

		// Override video type
		if (vrState.videoType == ProcessingVideoType::Force2D) {
			activeFormat.isVR = false;
		} else if (vrState.videoType == ProcessingVideoType::ForceVR) {
			activeFormat.isVR = true;
		}

		// Override layout
		if (vrState.vrLayout == ProcessingVRLayout::ForceSBS) {
			activeFormat.layout = VRLayout::SideBySide;
		} else if (vrState.vrLayout == ProcessingVRLayout::ForceTB) {
			activeFormat.layout = VRLayout::TopBottom;
		}

		bool isVR = activeFormat.isVR;
		bool needsProcessing = isVR && activeFormat.layout != VRLayout::None;

		uint32_t finalTexture = ctx->processingTexture;  // Default: use raw mpv output

		if (needsProcessing && ctx->cropShader && ctx->quadVAO) {
			// For AI tracking: Only crop VR video to single eye
			// NO unwarp shader - AI needs raw pixel data, not view-dependent projections
			// VR unwarp is only for human viewing, not for AI processing
			glBindFramebuffer(GL_FRAMEBUFFER, ctx->croppedFramebuffer);
			glViewport(0, 0, MpvPlayerContext::PROCESSING_SIZE, MpvPlayerContext::PROCESSING_SIZE);
			glClear(GL_COLOR_BUFFER_BIT);

			ctx->cropShader->Use();
			int layoutType = (activeFormat.layout == VRLayout::SideBySide) ? 0 : 1;
			ctx->cropShader->SetLayout(layoutType);
			ctx->cropShader->SetUseRightEye(vrState.useRightEye);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, ctx->processingTexture);
			glBindVertexArray(ctx->quadVAO);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glBindVertexArray(0);

			// Use cropped texture (raw single eye, no unwarp)
			finalTexture = ctx->croppedTexture;

			// Reset state
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		// Async PBO readback (double-buffered, non-blocking)
		// Read from previous frame, start readback for current frame
		int readIndex = ctx->processingPBOIndex;
		int writeIndex = (ctx->processingPBOIndex + 1) % 2;

		// Bind the final texture for reading (raw or cropped for VR)
		glBindTexture(GL_TEXTURE_2D, finalTexture);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, ctx->processingPBO[writeIndex]);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

		// Map read PBO to get data from previous frame (may block if not ready)
		glBindBuffer(GL_PIXEL_PACK_BUFFER, ctx->processingPBO[readIndex]);
		const uint8_t* frameData = (const uint8_t*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

		if (frameData) {

			// Emit event with processing frame data
			double timeSeconds = ctx->data.duration * ctx->data.percentPos;
			EV::Enqueue<ProcessingFrameReadyEvent>(
				frameData,
				MpvPlayerContext::PROCESSING_SIZE,
				MpvPlayerContext::PROCESSING_SIZE,
				timeSeconds,
				ctx->playerType,
				ctx->data.videoWidth,
				ctx->data.videoHeight
			);

			LOG_INFO("ProcessingFrameReadyEvent enqueued");

			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		} else {
			LOG_ERROR("Failed to map PBO for frame readback");
		}

		// Unbind PBO
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		// Swap PBO index for next frame
		ctx->processingPBOIndex = writeIndex;
	}
}

void OFS_Videoplayer::Update(float delta) noexcept
{
    static bool logged = false;
    if (!logged) {
        LOG_INFO("OFS_Videoplayer::Update() called for first time");
        logged = true;
    }
    while(SDL_AtomicGet(&CTX->hasEvents) > 0) {
        ProcessEvents(CTX);
        SDL_AtomicDecRef(&CTX->hasEvents);
    }

    while(SDL_AtomicGet(&CTX->renderUpdate) > 0)
    {
        uint64_t flags = mpv_render_context_update(CTX->mpvGL);
	    if (flags & MPV_RENDER_UPDATE_FRAME) {
            RenderFrameToTexture(CTX);
        }
        SDL_AtomicDecRef(&CTX->renderUpdate);
    }
}

void OFS_Videoplayer::SetVolume(float volume) noexcept
{
    CTX->data.currentVolume = volume;
    stbsp_snprintf(CTX->tmpBuf.data(), CTX->tmpBuf.size(), "%.2f", (float)(volume*100.f));
    const char* cmd[]{"set", "volume", CTX->tmpBuf.data(), NULL};
    mpv_command_async(CTX->mpv, 0, cmd);
}

void OFS_Videoplayer::NextFrame() noexcept
{
    if (IsPaused()) {
        // use same method as previousFrame for consistency
        double relSeek = FrameTime() * 1.000001;
        CTX->data.percentPos += (relSeek / CTX->data.duration);
        CTX->data.percentPos = Util::Clamp(CTX->data.percentPos, 0.0, 1.0);
        SetPositionPercent(CTX->data.percentPos, false);
    }
}

void OFS_Videoplayer::PreviousFrame() noexcept
{
    if (IsPaused()) {
        // this seeks much faster
        // https://github.com/mpv-player/mpv/issues/4019#issuecomment-358641908
        double relSeek = FrameTime() * 1.000001;
        CTX->data.percentPos -= (relSeek / CTX->data.duration);
        CTX->data.percentPos = Util::Clamp(CTX->data.percentPos, 0.0, 1.0);
        SetPositionPercent(CTX->data.percentPos, false);
    }
}

void OFS_Videoplayer::OpenVideo(const std::string& path) noexcept
{
    LOGF_INFO("Opening video: \"%s\"", path.c_str());
    CloseVideo();
    
    const char* cmd[] = { "loadfile", path.c_str(), NULL };
    mpv_command_async(CTX->mpv, 0, cmd);
    
    MpvDataCache newCache;
    newCache.currentSpeed = CTX->data.currentSpeed;
    newCache.paused = CTX->data.paused;
    CTX->data = newCache;
    CTX->vrDetectionDone = false;  // Reset VR detection for new video

    SetPaused(true);
    SetVolume(CTX->data.currentVolume);
    SetSpeed(CTX->data.currentSpeed);
}

void OFS_Videoplayer::SetSpeed(float speed) noexcept
{
    speed = Util::Clamp<float>(speed, MinPlaybackSpeed, MaxPlaybackSpeed);
    if (CurrentSpeed() != speed) {
        stbsp_snprintf(CTX->tmpBuf.data(), CTX->tmpBuf.size(), "%.3f", speed);
        const char* cmd[]{ "set", "speed", CTX->tmpBuf.data(), NULL };
        mpv_command_async(CTX->mpv, 0, cmd);
    }
}

void OFS_Videoplayer::AddSpeed(float speed) noexcept
{
    speed += CTX->data.currentSpeed;
    speed = Util::Clamp<float>(speed, MinPlaybackSpeed, MaxPlaybackSpeed);
    SetSpeed(speed);
}

void OFS_Videoplayer::SetPositionPercent(float percentPos, bool pausesVideo) noexcept
{
    logicalPosition = percentPos;
    CTX->data.percentPos = percentPos;
    stbsp_snprintf(CTX->tmpBuf.data(), CTX->tmpBuf.size(), "%.08f", (float)(percentPos * 100.0f));
    const char* cmd[]{ "seek", CTX->tmpBuf.data(), "absolute-percent+exact", NULL };
    if (pausesVideo) {
        SetPaused(true);
    }
    mpv_command_async(CTX->mpv, 0, cmd);
}

void OFS_Videoplayer::SetPositionExact(float timeSeconds, bool pausesVideo) noexcept
{
    // this updates logicalPosition in SetPositionPercent
    timeSeconds = Util::Clamp<float>(timeSeconds, 0.f, Duration());
    float relPos = ((float)timeSeconds) / Duration();
    SetPositionPercent(relPos, pausesVideo);
}

void OFS_Videoplayer::SeekRelative(float timeSeconds) noexcept
{
    // this updates logicalPosition in SetPositionPercent
    auto seekTo = CurrentTime() + timeSeconds;
    seekTo = std::max(seekTo, 0.0);
    SetPositionExact(seekTo);
}

void OFS_Videoplayer::SeekFrames(int32_t offset) noexcept
{
    // this updates logicalPosition in SetPositionPercent
    if (IsPaused()) {
        float relSeek = (FrameTime() * 1.000001f) * offset;
        CTX->data.percentPos += (relSeek / CTX->data.duration);
        CTX->data.percentPos = Util::Clamp(CTX->data.percentPos, 0.0, 1.0);
        SetPositionPercent(CTX->data.percentPos, false);
    }
}

void OFS_Videoplayer::SetPaused(bool paused) noexcept
{
    if ((bool)CTX->data.paused == paused) return;
    int64_t setPaused = paused;
    mpv_set_property_async(CTX->mpv, 0, "pause", MPV_FORMAT_FLAG, &setPaused);
}

void OFS_Videoplayer::CycleSubtitles() noexcept
{
    const char* cmd[]{ "cycle", "sub", NULL};
    mpv_command_async(CTX->mpv, 0, cmd);
}

void OFS_Videoplayer::CloseVideo() noexcept
{
    CTX->data.videoLoaded = false;
    const char* cmd[] = { "stop", NULL };
    mpv_command_async(CTX->mpv, 0, cmd);
    SetPaused(true);
}

void OFS_Videoplayer::NotifySwap() noexcept
{
    mpv_render_context_report_swap(CTX->mpvGL);
}

void OFS_Videoplayer::SaveFrameToImage(const std::string& directory) noexcept
{
    std::stringstream ss;
    auto currentFile = Util::PathFromString(VideoPath());
    std::string filename = currentFile.filename().replace_extension("").string();
    std::array<char, 15> tmp;
    double time = CurrentTime();
    Util::FormatTime(tmp.data(), tmp.size(), time, true);
    std::replace(tmp.begin(), tmp.end(), ':', '_');
    ss << filename << '_' << tmp.data() << ".png";
    if(!Util::CreateDirectories(directory)) {
        return;
    }
    auto dir = Util::PathFromString(directory);
    dir.make_preferred();
    std::string finalPath = (dir / ss.str()).string();
    const char* cmd[]{ "screenshot-to-file", finalPath.c_str(), NULL };
    mpv_command_async(CTX->mpv, 0, cmd);
}

// ==================== Getter ==================== 

uint16_t OFS_Videoplayer::VideoWidth() const noexcept
{
    return CTX->data.videoWidth;
}

uint16_t OFS_Videoplayer::VideoHeight() const noexcept
{
    return CTX->data.videoHeight;
}

float OFS_Videoplayer::FrameTime() const noexcept
{
    return CTX->data.averageFrameTime;
}

float OFS_Videoplayer::CurrentSpeed() const noexcept
{
    return CTX->data.currentSpeed;
}

float OFS_Videoplayer::Volume() const noexcept
{
    return CTX->data.currentVolume;
}

double OFS_Videoplayer::Duration() const noexcept
{
    return CTX->data.duration;
}

bool OFS_Videoplayer::IsPaused() const noexcept
{
    return CTX->data.paused;
}

float OFS_Videoplayer::Fps() const noexcept
{
    return CTX->data.fps;
}

bool OFS_Videoplayer::VideoLoaded() const noexcept
{
    return CTX->data.videoLoaded;
}

float OFS_Videoplayer::CurrentPercentPosition() const noexcept
{
    return logicalPosition;
}

double OFS_Videoplayer::CurrentTime() const noexcept
{
    if(CTX->data.paused)
    {
        return logicalPosition * CTX->data.duration;
    }
    else 
    {
        float timeSinceLastUpdate = (SDL_GetTicks64() - CTX->smoothTimer) / 1000.f;
        float positionOffset = (timeSinceLastUpdate * CTX->data.currentSpeed) / Duration();
        return (logicalPosition + positionOffset) * CTX->data.duration;
    }
}

double OFS_Videoplayer::CurrentPlayerPosition() const noexcept
{
    return CTX->data.percentPos;
}

const char* OFS_Videoplayer::VideoPath() const noexcept
{
    return CTX->data.filePath.c_str();
}

void OFS_Videoplayer::SetTrackingActive(bool active) noexcept
{
	CTX->trackingActive = active;
	if (active) {
		LOG_INFO("AI tracking enabled - processing pipeline active");
	} else {
		LOG_INFO("AI tracking disabled - processing pipeline inactive");
	}
}

bool OFS_Videoplayer::IsTrackingActive() const noexcept
{
	return CTX->trackingActive;
}
