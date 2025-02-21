project(AkGuitarDistortionFX)

set(SRC_FILES
    "../../Common/AkDistortion.cpp"
    "../../Common/AkMultiBandEQ.cpp"
    "../../Common/AkRectifier.cpp"
    "../Common/Mix2Interp.cpp"
    "../Common/OnePoleZeroHPFilter.cpp"
    "AkGuitarDistortionFX.cpp"
    "AkGuitarDistortionFXParams.cpp"
    "InitAkGuitarDistortionFX.cpp"
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
