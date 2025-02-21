project(AkSink)

set(SRC_FILES
    "Win32/AkSinkXAudio.cpp"
    "Win32/SinkPluginParams.cpp"
)

add_library(${PROJECT_NAME} STATIC ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
    "Win32"
    "../../../../../include"
    "../../../../../dxsdk(June2010)/Include"
)

target_link_libraries(${PROJECT_NAME})
