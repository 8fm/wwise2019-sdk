project(AkSineSource)

set(SRC_FILES
    "../../../../DSP/AkSineOsc.cpp"
    "AkFXSrcSine.cpp"
    "AkFXSrcSineParams.cpp"
)

add_library(${PROJECT_NAME} STATIC ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
    "."
    "../../../../../include"
    "../../../../DSP"
)

target_link_libraries(${PROJECT_NAME})
