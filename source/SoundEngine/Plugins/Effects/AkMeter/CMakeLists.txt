project(AkMeterFX)

set(SRC_FILES
    "AkMeterFX.cpp"
    "AkMeterFXParams.cpp"
    "AkMeterManager.cpp"
    "InitAkMeterFX.cpp"
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
    "../../../AkAudiolib/Win32"
    "../../../AkAudiolib/Common"
)

target_link_libraries(${PROJECT_NAME})
