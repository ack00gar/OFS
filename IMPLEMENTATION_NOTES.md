# OFS-FunGen Implementation Notes

**Date**: 2025-11-10
**Status**: Event System Refactoring - Phase 0 Complete

---

## Recent Changes

### Phase 0: Memory Safety Improvements

#### ID-Based Event Handling

Replaced raw pointer-based event handling with ID-based lookup to eliminate use-after-free vulnerabilities.

**Changes Made:**

1. **Event Classes** (`OFS-lib/Funscript/Funscript.h`)
   - `FunscriptActionsChangedEvent`: Now stores scriptId, scriptName, actionCount
   - `FunscriptSelectionChangedEvent`: Now stores scriptId, scriptName, selectedCount
   - `FunscriptNameChangedEvent`: Now stores scriptId, newName, oldName

2. **Script Registry** (`src/OFS_Project.h/cpp`)
   - Added `scriptRegistry` map for ID→script lookup
   - `RegisterScript()`: Assigns unique ID to scripts
   - `GetScriptById()`: Safe ID-based lookup
   - `UnregisterScript()`: Cleanup on removal
   - Auto-registration in constructor, Load(), AddFunscript()
   - Auto-unregistration in RemoveFunscript()

3. **Event Emitters** (`OFS-lib/Funscript/Funscript.cpp`)
   - `Funscript::Update()`: Emits events with ID and metadata
   - `Funscript::UpdateRelativePath()`: Emits name changes with old/new names

4. **Event Handlers**
   - `OpenFunscripter::FunscriptChanged()`: Uses ID-based lookup (`src/OpenFunscripter.cpp`)
   - `OFS_WebsocketApi` handlers: Use ID-based lookup for actions and name events (`src/api/OFS_WebsocketApi.cpp`)

**Benefits:**
- Eliminates dangling pointer access
- Safer script lifecycle management
- Foundation for future tracking integration

---

## Architecture Overview

### Dual-Pipeline Infrastructure

The codebase includes a dual-pipeline video processing architecture:

1. **Display Pipeline**: Full resolution rendering for UI
2. **Processing Pipeline**: Downscaled frames (640×640) for tracking/analysis

**Key Components:**
- Dual-FBO rendering system
- VR format detection and unwarp
- Async PBO readback for frame data
- ProcessingFrameReadyEvent for frame distribution

### VR Support

Complete VR video processing pipeline:
- Auto-detection of SBS/TB layouts
- Equirect 180°/360° and Fisheye 190°/200° support
- GPU-accelerated cropping and unwarp
- Configurable pitch/yaw/zoom

---

## Development Notes

### Building
- macOS: Use `./build-macos.sh`
- Clean builds recommended after header changes
- Binary location: `bin/OpenFunscripter.app/Contents/MacOS/OpenFunscripter`

### Testing
- Test project load/save cycles
- Verify script add/remove operations
- Check WebSocket API script updates
- Validate VR video playback

---

## Future Work

- Additional event system improvements
- Performance optimization opportunities
- Tracking system integration
- Enhanced VR controls
