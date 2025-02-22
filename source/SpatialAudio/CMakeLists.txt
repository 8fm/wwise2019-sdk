project(AkSpatialAudio)

set(SRC_FILES
    "Common/AkAcousticPortal.cpp"
    "Common/AkAcousticRoom.cpp"
    "Common/AkDiffractionEdge.cpp"
    "Common/AkDiffractionPath.cpp"
    "Common/AkGeometrySet.cpp"
    "Common/AkImageSource.cpp"
    "Common/AkReflectInstance.cpp"
    "Common/AkScene.cpp"
    "Common/AkSoundGeometry.cpp"
    "Common/AkSpatialAudio.cpp"
    "Common/AkSpatialAudioCompts.cpp"
    "Common/AkSpatialAudioTasks.cpp"
    "Common/AkStochasticPath.cpp"
    "Common/AkStochasticReflectionEngine.cpp"
)
if (WIN32)
    set(STDAFX_H "Win32/stdafx.h")
    set(SYSTEM_INC "Win32" "../SoundEngine/AkAudiolib/Win32")
    list(APPEND SRC_FILES
        "Win32/stdafx.cpp"
    )
else()
    set(STDAFX_H "Linux/stdafx.h")
    set(SYSTEM_INC
        "Linux"
        "POSIX"
        "../SoundEngine/AkAudiolib/Linux"
        "../SoundEngine/AkAudiolib/POSIX"
    )
endif()

add_library(${PROJECT_NAME} STATIC ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${SYSTEM_INC}
    "../../include"
    "Common"
    "../SoundEngine/AkAudiolib/Common"
    "../SoundEngine/AkAudiolib/SoftwarePipeline"
    "../SoundEngine/Plugins/Effects/Common"
)

target_link_libraries(${PROJECT_NAME})

target_precompile_headers(${PROJECT_NAME}
    PRIVATE
    ${STDAFX_H}
)
