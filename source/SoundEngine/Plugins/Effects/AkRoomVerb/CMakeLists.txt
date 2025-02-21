project(AkRoomVerbFX)

set(SRC_FILES
    "../Common/AllpassFilter.cpp"
    "../Common/ConstantPowerChannelMixdown.cpp"
    "../Common/DelayLine.cpp"
    "../Common/Mix2Interp.cpp"
    "../Common/Mix3Interp.cpp"
    "../Common/OnePoleZeroHPFilter.cpp"
    "AkRoomVerbFX.cpp"
    "AkRoomVerbFXParams.cpp"
    "ERUnitDual.cpp"
    "FDN4.cpp"
    "InitAkRoomVerbFX.cpp"
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
)

target_link_libraries(${PROJECT_NAME})
