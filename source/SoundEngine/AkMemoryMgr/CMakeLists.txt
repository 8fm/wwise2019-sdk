project(AkMemoryMgr)

set(C_SRC
    "../3rdParty/Common/rpmalloc/rpmalloc.c"
)
set(SRC_FILES
    "Common/AkMemoryMgr.cpp"
    "Common/AkMemoryMgrBase.cpp"
)
if (WIN32)
    set(STDAFX_H "Win32/stdafx.h")
    set(SYSTEM_INC "Win32" "../AkAudiolib/Win32")
    list(APPEND SRC_FILES
        "Win32/stdafx.cpp"
    )
else()
    set(STDAFX_H "Linux/stdafx.h")
    set(SYSTEM_INC "Linux" "../AkAudiolib/Linux")
endif()

set_source_files_properties(${C_SRC} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(APPEND SRC_FILES ${C_SRC})
add_library(${PROJECT_NAME} STATIC ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${SYSTEM_INC}
    "Common"
    "../3rdParty/Common"
    "../../../include"
    "../AkAudiolib/Common"
)

target_link_libraries(${PROJECT_NAME})

target_precompile_headers(${PROJECT_NAME}
    PRIVATE
    ${STDAFX_H}
)
