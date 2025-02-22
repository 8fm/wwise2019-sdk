project(IntegrationDemo)

set(SRC_FILES
    "../SoundEngine/Common/AkFileLocationBase.cpp"
    "../SoundEngine/Common/AkFilePackage.cpp"
    "../SoundEngine/Common/AkFilePackageLUT.cpp"
    "../SoundEngine/Common/AkMultipleFileLocation.cpp"
    "Common/Drawing.cpp"
    "Common/Helpers.cpp"
    "Common/IntegrationDemo.cpp"
    "Common/SoundInputBase.cpp"
    "Common/SoundInputMgrBase.cpp"
    "Common/UniversalInput.cpp"
    "DemoPages/BaseMenuPage.cpp"
    "DemoPages/Demo3dBus.cpp"
    "DemoPages/DemoBGM.cpp"
    "DemoPages/DemoDialogueMenu.cpp"
    "DemoPages/DemoDynamicDialogue.cpp"
    "DemoPages/DemoExternalSources.cpp"
    "DemoPages/DemoFootstepsManyVariables.cpp"
    "DemoPages/DemoInteractiveMusic.cpp"
    "DemoPages/DemoLocalization.cpp"
    "DemoPages/DemoMIDIMetronome.cpp"
    "DemoPages/DemoMarkers.cpp"
    "DemoPages/DemoMicrophone.cpp"
    "DemoPages/DemoMusicCallbacks.cpp"
    "DemoPages/DemoOptions.cpp"
    "DemoPages/DemoPositioning.cpp"
    "DemoPages/DemoPositioningMenu.cpp"
    "DemoPages/DemoRTPCCarEngine.cpp"
    "DemoPages/DemoSpatialAudio.cpp"
    "DemoPages/DemoSpatialAudioGeometry.cpp"
    "FreetypeRenderer/FreetypeGraphicRenderer.cpp"
    "MenuSystem/ButtonControl.cpp"
    "MenuSystem/Control.cpp"
    "MenuSystem/Menu.cpp"
    "MenuSystem/MessagePage.cpp"
    "MenuSystem/MovableChip.cpp"
    "MenuSystem/MultiplayerPage.cpp"
    "MenuSystem/NumericControl.cpp"
    "MenuSystem/Page.cpp"
    "MenuSystem/ToggleControl.cpp"
)

set(INTEGRATION_LIBS
    AkTremoloFX
    AkMemoryMgr
    AkSineSource
    AkStereoDelayFX
    AkGuitarDistortionFX
    AkSoundEngine
    AkRoomVerbFX
    AkParametricEQFX
    AkToneSource
    AkFlangerFX
    AkCompressorFX
    AkAudioInputSource
    AkMusicEngine
    AkDelayFX
    AkPitchShifterFX
    AkRecorderFX
    AkSilenceSource
    AkSynthOneSource
    AkGainFX
    AkOpusDecoder
    AkVorbisDecoder
    AkTimeStretchFX
    AkMatrixReverbFX
    AkMeterFX
    AkSpatialAudio
    AkStreamMgr
    AkHarmonizerFX
    AkPeakLimiterFX
    AkExpanderFX
)

if (NOT ${GAME_CONFIG} STREQUAL "Final") # CommunicationCentral
    list(APPEND INTEGRATION_LIBS CommunicationCentral)
endif()

if (WIN32)
    set(PIXELSHADER_FILES
        "Windows/Shaders/Shader_Line2DSimplePs.hlsl"
        "Windows/Shaders/Shader_Tex2DSimplePs.hlsl"
    )
    set(VERTEXSHADER_FILES
        "Windows/Shaders/Shader_Line2DSimpleVs.hlsl"
        "Windows/Shaders/Shader_Tex2DSimpleVs.hlsl"
    )

    set_property(SOURCE ${PIXELSHADER_FILES} PROPERTY VS_SHADER_TYPE Pixel)
    set_property(SOURCE ${PIXELSHADER_FILES} PROPERTY VS_SHADER_ENTRYPOINT PsMain)
    set_property(SOURCE ${PIXELSHADER_FILES} PROPERTY VS_SHADER_MODEL 5.0)
    set_property(SOURCE ${PIXELSHADER_FILES} PROPERTY VS_SHADER_OUTPUT_HEADER_FILE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shader/%(Filename).h")

    set_property(SOURCE ${VERTEXSHADER_FILES} PROPERTY VS_SHADER_TYPE Vertex)
    set_property(SOURCE ${VERTEXSHADER_FILES} PROPERTY VS_SHADER_ENTRYPOINT VsMain)
    set_property(SOURCE ${VERTEXSHADER_FILES} PROPERTY VS_SHADER_MODEL 5.0)
    set_property(SOURCE ${VERTEXSHADER_FILES} PROPERTY VS_SHADER_OUTPUT_HEADER_FILE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shader/%(Filename).h")

    list(APPEND SRC_FILES ${PIXELSHADER_FILES} ${VERTEXSHADER_FILES})

    set(STDAFX_H "Windows/stdafx.h")
    set(SYSTEM_INC
        "Windows"
        "../SoundEngine/Win32"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shader"
    )
    list(APPEND SRC_FILES
        "../SoundEngine/Win32/AkDefaultIOHookBlocking.cpp"
        "../SoundEngine/Win32/AkDefaultIOHookDeferred.cpp"
        "Windows/DemoOptionsUtil.cpp"
        "Windows/InputMgr.cpp"
        "Windows/Main.cpp"
        "Windows/Platform.cpp"
        "Windows/Render_D3D11.cpp"
        "Windows/Render_D3D12.cpp"
        "Windows/SoundInput.cpp"
        "Windows/SoundInputMgr.cpp"
        "Windows/stdafx.cpp"
    )
    list(APPEND INTEGRATION_LIBS
        AkSink
        ws2_32
        dinput8
        XInput
        Winmm
        d3d11
    )
else()
    set(STDAFX_H "Linux/stdafx.h")
    set(SYSTEM_INC
        "Linux"
        "../SoundEngine/POSIX"
    )
    list(APPEND SRC_FILES
        "../SoundEngine/POSIX/AkDefaultIOHookBlocking.cpp"
        "../SoundEngine/POSIX/AkDefaultIOHookDeferred.cpp"
        "Linux/DemoOptionsUtil.cpp"
        "Linux/InputMgr.cpp"
        "Linux/Main.cpp"
        "Linux/Render_SDL.cpp"
        "Linux/SoundInput.cpp"
        "Linux/SoundInputMgr.cpp"
    )
    list(APPEND INTEGRATION_LIBS
        SDL2
    )
endif()

add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${SYSTEM_INC}
    "Common"
    "FreetypeRenderer"
    "FreetypeRenderer/Assets"
    "MenuSystem"
    "../SoundEngine/Common"
    "../../include"
    "DemoPages"
    "WwiseProject/GeneratedSoundBanks"
)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} ${INTEGRATION_LIBS})
else()
    target_link_libraries(${PROJECT_NAME}
        "$<LINK_GROUP:RESCAN,${INTEGRATION_LIBS}>"
    )
endif()

target_precompile_headers(${PROJECT_NAME}
    PRIVATE
    ${STDAFX_H}
)
