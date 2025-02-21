project(AkPitchShifterFX)

set(SRC_FILES
    "../../Common/AkDelayPitchShift.cpp"
    "../Common/DelayLineLight.cpp"
    "../Common/Mix2Interp.cpp"
    "AkPitchShifterDSPProcess.cpp"
    "AkPitchShifterFX.cpp"
    "AkPitchShifterFXParams.cpp"
    "InitAkPitchShifterFX.cpp"
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
