project(AkTremoloFX)

set(SRC_FILES
    "../Common/LFO.cpp"
    "AkTremoloFX.cpp"
    "AkTremoloFXParams.cpp"
    "InitAkTremoloFX.cpp"
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
