# Development Workflow

## Iterating on Work Branch Changes

This document describes the workflow for progressively applying changes from a work branch to main.

### Overview

When working with external assistance branches (like the `claude/analyze-core-gui-defects-*` branches), changes should be progressively merged into main in logical, testable increments rather than all at once.

### Step-by-Step Process

#### 1. Identify Changes

First, examine what changed in the work branch:

```bash
# Compare work branch to base
git fetch origin
git diff <base-commit>..<work-branch-commit> --stat
git log <base-commit>..<work-branch-commit> --oneline
```

Example:
```bash
git diff 1d3352d..da86811 --stat
```

#### 2. Review Each File's Changes

For each modified file, review the actual changes:

```bash
git diff <base-commit>..<work-branch-commit> -- <file-path>
```

Example:
```bash
git diff 1d3352d..da86811 -- OFS-lib/Funscript/Funscript.h
```

#### 3. Apply Changes to Main

Checkout main and apply changes file by file:

```bash
# Switch to main
git checkout main

# For each file with changes:
# 1. Read the current file
# 2. Apply the diff manually using Edit operations
# 3. Verify the changes compile

# Example workflow:
# - Read: OFS-lib/Funscript/Funscript.h
# - Apply edits based on diff
# - Test compile
```

#### 4. Sanitize Documentation

When work branches include documentation or comments with external references:

- Remove branch names
- Remove external service references
- Keep technical content
- Focus on "what" and "why", not "who" or "where"

Example: Convert detailed analysis docs into concise `IMPLEMENTATION_NOTES.md`

#### 5. Build and Test

```bash
# Clean build
rm -rf build build-debug build-release build-temp

# Build
./build-macos.sh

# Install for testing (macOS)
rm -rf /Applications/OpenFunscripter.app
cp -R bin/OpenFunscripter.app /Applications/

# Test the application
# - Load projects
# - Add/remove scripts
# - Test WebSocket API if modified
# - Verify VR features if touched
```

#### 6. Commit

Create a detailed commit message:

```bash
git add <modified-files>
git commit -m "type: Brief summary

Detailed description of what changed and why.

Changes:
- List key modifications
- Include file locations
- Explain impact

Benefits:
- What this fixes
- What this enables
"
```

### Example: Applying Phase 0 Changes

The recent commit demonstrates this workflow:

1. **Identified changes**: 6 code files + 2 doc files modified
2. **Reviewed diffs**: Used `git diff` to see exact changes
3. **Applied incrementally**:
   - Updated event classes (Funscript.h)
   - Modified event emitters (Funscript.cpp)
   - Added registry system (OFS_Project.h/cpp)
   - Updated event handlers (OpenFunscripter.cpp, OFS_WebsocketApi.cpp)
4. **Sanitized docs**: Created IMPLEMENTATION_NOTES.md without external references
5. **Built & tested**: Clean build, verified binary
6. **Committed**: Detailed commit message explaining all changes

### Future Work Branch Changes

When the work branch receives additional commits:

1. **Identify new commits**: `git log <last-merged-commit>..origin/<work-branch>`
2. **Review changes**: `git diff <last-merged-commit>..<new-commit>`
3. **Repeat process**: Apply → Test → Commit
4. **Document in IMPLEMENTATION_NOTES.md**: Update with new changes

### Branch Management

```bash
# Check current work branch status
git fetch origin
git log main..origin/<work-branch> --oneline

# After merging changes to main
git push origin main

# Keep feature branches updated if needed
git checkout feature/dual-pipeline-tracking
git rebase main
```

### Key Principles

1. **Small increments**: Merge logical units of work
2. **Always compile**: Every intermediate state should build
3. **Test before commit**: Verify functionality works
4. **Document clearly**: Explain what, why, and impact
5. **Sanitize references**: Remove external branch/service names
6. **Preserve intent**: Keep technical content and reasoning

---

## Current State

- **Base branch**: `main` (a34b264)
- **Feature branch**: `feature/dual-pipeline-tracking` (1d3352d)
- **Work branch**: `origin/claude/analyze-core-gui-defects-011CUxyu3g7YdhRqchvAu8eW` (da86811)
- **Last applied**: Phase 0 - ID-based event handling (commit e8fa078)
- **Pending**: Phases 1-10 from work branch (if any additional changes exist)
