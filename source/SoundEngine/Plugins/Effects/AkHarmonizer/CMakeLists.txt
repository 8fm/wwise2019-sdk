project(AkHarmonizerFX)

add_definitions(-DUSEALLBUTTERFLIES)
set(SRC_FILES
    "../../../AkAudiolib/Common/ak_fft/ak_fft.cpp"
    "../../../AkAudiolib/Common/ak_fft/tools/ak_fftr.cpp"
    "../../Common/AkPhaseVocoder.cpp"
    "../../Common/AkResamplingPhaseVocoder.cpp"
    "../../Common/CircularBuffer.cpp"
    "../../Common/FreqWindow.cpp"
    "../../Common/OLACircularBuffer.cpp"
    "../../Common/ResamplingCircularBuffer.cpp"
    "../../Common/TimeWindow.cpp"
    "../Common/DelayLineLight.cpp"
    "../Common/Mix2Interp.cpp"
    "AkHarmonizerDSPProcess.cpp"
    "AkHarmonizerFX.cpp"
    "AkHarmonizerFXParams.cpp"
    "InitAkHarmonizerFX.cpp"
)
if (WIN32)
    set(SYSTEM_INC "../Common/Win32")
else()
    set(SYSTEM_INC "../Common/Linux")
endif()

add_library(${PROJECT_NAME} STATIC ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
    "."
    "../../../../../include"
    ${SYSTEM_INC}
    "../Common"
    "../../Common"
    "../../../AkAudiolib/Common"
    "../../../AkAudiolib/Common/ak_fft"
    "../../../AkAudiolib/Common/ak_fft/tools"
)

target_link_libraries(${PROJECT_NAME})
