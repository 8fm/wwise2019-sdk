project(AkRecorderFX)

set(SRC_FILES
    "AkRecorderFX.cpp"
    "AkRecorderFXParams.cpp"
    "AkRecorderManager.cpp"
    "InitAkRecorderFX.cpp"
)
if (WIN32)
    set(SYSTEM_INC
        "../Common/Win32"
        "../../../AkAudiolib/Win32"
    )
else()
    set(SYSTEM_INC
        "../Common/Linux"
        "../../../AkAudiolib/Linux"
    )
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
