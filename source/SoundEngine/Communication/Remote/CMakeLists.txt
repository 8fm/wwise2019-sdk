project(CommunicationCentral)

add_definitions(-DPROXYCENTRAL_CONNECTED)
if (MSVC)
    add_compile_options(/wd4355)
endif()

set(SRC_FILES
    "../Common/ConsoleDiscoveryMessage.cpp"
    "../Common/Serializer.cpp"
    "CodeBase/BaseChannel.cpp"
    "CodeBase/CommandChannel.cpp"
    "CodeBase/Communication.cpp"
    "CodeBase/CommunicationCentral.cpp"
    "CodeBase/DiscoveryChannel.cpp"
    "CodeBase/IncomingChannel.cpp"
    "CodeBase/NotificationChannel.cpp"
    "PC/GameSocketAddr.cpp"
)

if (WIN32)
    set(STDAFX_H "PC/stdafx.h")
    set(SYSTEM_INC "PC")
    list(APPEND SRC_FILES
        "PC/GameSocket.cpp"
        "PC/Network.cpp"
        "PC/stdafx.cpp"
    )
else()
    set(STDAFX_H "Linux/stdafx.h")
    set(SYSTEM_INC "Linux")
    list(APPEND SRC_FILES
        "POSIX/GameSocket.cpp"
        "POSIX/Network.cpp"
    )
endif()

add_library(${PROJECT_NAME} STATIC ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${SYSTEM_INC}
    "../Common"
    "CodeBase"
    "../../../../include"
    "../../SoundEngineProxy/Remote/CodeBase"
    "../../SoundEngineProxy/Common"
    "../../AkAudiolib/PC"
    "../../AkAudiolib/Common"
)

target_link_libraries(${PROJECT_NAME})

target_precompile_headers(${PROJECT_NAME}
    PRIVATE
    ${STDAFX_H}
)
