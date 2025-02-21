project(AkSynthOneSource)

set(SRC_FILES
    "../../Common/AkColoredNoise.cpp"
    "../../Common/AkWhiteNoise.cpp"
    "../../Effects/Common/OnePoleZeroHPFilter.cpp"
    "AkSynthOne.cpp"
    "AkSynthOneDsp.cpp"
    "AkSynthOneParams.cpp"
    "InitAkSynthOne.cpp"
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
    "../../Effects/Common"
    "../../../Common"
    "../../../AkAudiolib/Common"
)

target_link_libraries(${PROJECT_NAME})
