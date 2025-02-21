project(AkStereoDelayFX)

set(SRC_FILES
    "../Common/Mix2Interp.cpp"
    "../Common/Mix3Interp.cpp"
    "AkStereoDelayDSPProcess.cpp"
    "AkStereoDelayFX.cpp"
    "AkStereoDelayFXParams.cpp"
    "AkStereoDelayLine.cpp"
    "InitAkStereoDelayFX.cpp"
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
    "../../../../../include/AK/DSP"
    "../../../AkAudiolib/Common"
)

target_link_libraries(${PROJECT_NAME})
