project(OpenFunscripter)

set(OPEN_FUNSCRIPTER_SOURCES
  "main.cpp"
  "OpenFunscripter.cpp"
  "OFS_ScriptingMode.cpp"
  "OFS_Project.cpp"
  
  "OFS_UndoSystem.cpp"

  "UI/OFS_Preferences.cpp"
  "UI/OFS_ScriptSimulator.cpp"
  "UI/OFS_SpecialFunctions.cpp"
  "UI/OFS_ScriptPositionsOverlays.cpp"
  "UI/OFS_DownloadFfmpeg.cpp"
  "UI/OFS_FunscriptMetadataEditor.cpp"
  "UI/OFS_ChapterManager.cpp"

  "api/OFS_WebsocketApi.cpp"
  "api/OFS_WebsocketApiClient.cpp"
  "api/OFS_WebsocketApiEvents.cpp"
  "api/OFS_WebsocketApiCommands.cpp"

  "gl/OFS_GPU.cpp"

  "state/OpenFunscripterState.cpp"

  "lua/OFS_LuaExtensions.cpp"
  "lua/OFS_LuaExtension.cpp"
  "lua/OFS_LuaCoreExtension.cpp"
  "lua/OFS_LuaExtensionAPI.cpp"
  "lua/api/OFS_LuaPlayerAPI.cpp"
  "lua/api/OFS_LuaImGuiAPI.cpp"
  "lua/api/OFS_LuaScriptAPI.cpp"
  "lua/api/OFS_LuaProcessAPI.cpp"
)

if(WIN32)
	set(OPEN_FUNSCRIPTER_SOURCES ${OPEN_FUNSCRIPTER_SOURCES} "../icon.rc")
endif()

if(APPLE)
	set(MACOSX_BUNDLE_ICON_FILE icon.icns)
	set(MACOSX_BUNDLE_BUNDLE_NAME "OpenFunscripter")
	set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.fungen.openfunscripter")
	set(MACOSX_BUNDLE_BUNDLE_VERSION "3.2.1")
	set(MACOSX_BUNDLE_SHORT_VERSION_STRING "3.2.1")
	set(MACOSX_BUNDLE_COPYRIGHT "FunGen Edition")

	set(APP_ICON_MACOSX "${CMAKE_SOURCE_DIR}/data/icon.icns")
	set_source_files_properties(${APP_ICON_MACOSX} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

	add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${OPEN_FUNSCRIPTER_SOURCES} ${APP_ICON_MACOSX})
elseif(WIN32)
	add_executable(${PROJECT_NAME} WIN32 ${OPEN_FUNSCRIPTER_SOURCES})
else()
	add_executable(${PROJECT_NAME} ${OPEN_FUNSCRIPTER_SOURCES})
endif()

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR})

# copy data directory
if(APPLE)
	add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
						COMMAND ${CMAKE_COMMAND} -E copy_directory
						"${CMAKE_SOURCE_DIR}/data/" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/../Resources/data")
else()
	add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
						COMMAND ${CMAKE_COMMAND} -E copy_directory
						"${CMAKE_SOURCE_DIR}/data/" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data")
endif()


target_link_libraries(${PROJECT_NAME} PUBLIC
  lua
  sol2
  OFS_lib
  civetweb
)

if(WIN32)
  # download mpv binaries
  # Using latest mpv build from zhongfly (updates daily)
  # To manually use a specific version, download from:
  # https://github.com/zhongfly/mpv-winbuild/releases
  if (NOT EXISTS "${CMAKE_BINARY_DIR}/mpv-2.dll")
    message(STATUS "Fetching latest Windows mpv build info...")
    file(DOWNLOAD "https://api.github.com/repos/zhongfly/mpv-winbuild/releases/latest"
         "${CMAKE_BINARY_DIR}/mpv-latest.json" SHOW_PROGRESS)

    file(READ "${CMAKE_BINARY_DIR}/mpv-latest.json" MPV_JSON)
    string(REGEX MATCH "\"browser_download_url\"[^\"]*\"([^\"]*mpv-dev-x86_64-v3[^\"]*\\.7z)\"" _ ${MPV_JSON})
    set(MPV_DOWNLOAD_URL ${CMAKE_MATCH_1})

    if(MPV_DOWNLOAD_URL)
      message(STATUS "Downloading mpv from: ${MPV_DOWNLOAD_URL}")
      file(DOWNLOAD "https://www.7-zip.org/a/7zr.exe" "7zr.exe" SHOW_PROGRESS)
      file(DOWNLOAD ${MPV_DOWNLOAD_URL} "mpv-dev.7z" SHOW_PROGRESS)
      execute_process(
        COMMAND "${CMAKE_BINARY_DIR}/7zr.exe" "x" "-y" "mpv-dev.7z" "mpv-2.dll"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        RESULT_VARIABLE EXTRACT_RESULT
      )
      if(NOT EXTRACT_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to extract mpv-2.dll")
      endif()
    else()
      message(FATAL_ERROR "Could not find mpv download URL. Please manually download mpv-dev-x86_64-v3-*.7z from https://github.com/zhongfly/mpv-winbuild/releases")
    endif()
  endif()

	# copy mpv-2.dll to bin dir
	add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   "${CMAKE_BINARY_DIR}/mpv-2.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/mpv-2.dll"
				   DEPENDS "${CMAKE_BINARY_DIR}/mpv-2.dll")
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC
	"_CRT_SECURE_NO_WARNINGS"
)

target_include_directories(${PROJECT_NAME} PRIVATE 
	"${PROJECT_SOURCE_DIR}/"
	"${PROJECT_SOURCE_DIR}/UI/"
	"${PROJECT_SOURCE_DIR}/Funscript/"
	"${PROJECT_SOURCE_DIR}/gl/"
	"${PROJECT_SOURCE_DIR}/event/"
	"${PROJECT_SOURCE_DIR}/lua/"
	"${PROJECT_SOURCE_DIR}/api/"
)

# c++17
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

if(WIN32)
	if(CMAKE_BUILD_TYPE MATCHES "Release" OR CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo") 
		target_compile_options(${PROJECT_NAME} PUBLIC /GL)
		target_link_options(${PROJECT_NAME} PUBLIC /LTCG)
		message("== ${PROJECT_NAME} - Whole program optimization enabled.")
	endif()
elseif(UNIX AND NOT APPLE) # clang/gcc
    target_compile_options(${PROJECT_NAME} PUBLIC -fpermissive)
	install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "bin/")
elseif(APPLE)
	target_compile_options(${PROJECT_NAME} PUBLIC -fpermissive)
	# Note Mac specific extension .app
	set(APPS "\${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app")
endif()


if(OFS_SNAP_IMAGE)
# this is awful
target_compile_definitions(${PROJECT_NAME} PRIVATE
	"OFS_SNAP_IMAGE")
install(DIRECTORY "../data" DESTINATION "bin/")
endif()
