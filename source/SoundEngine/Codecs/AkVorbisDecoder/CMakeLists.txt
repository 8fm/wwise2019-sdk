project(AkVorbisDecoder)

if (MSVC)
    add_compile_options(/wd4244 /wd4305 /wd4267)
endif()

set(SRC_FILES
    "Common/AkSrcBankVorbis.cpp"
    "Common/AkSrcFileVorbis.cpp"
    "Common/AkVorbisCodebookMgr.cpp"
    "Common/AkVorbisLib.cpp"
    "Tremor/FloatingPoint/AkVorbisCodec.cpp"
    "Tremor/FloatingPoint/codebook.cpp"
    "Tremor/FloatingPoint/floor1.cpp"
    "Tremor/SIMD/SinCosGen.cpp"
    "Tremor/SIMD/dsp.cpp"
    "Tremor/SIMD/mdct_SIMD.cpp"
    "Tremor/akvorbis_bitwise.cpp"
    "Tremor/akvorbis_info.cpp"
    "Tremor/mapping0.cpp"
    "Tremor/packed_codebooks.cpp"
    "Tremor/res012.cpp"
)
if (WIN32)
    set(SYSTEM_INC "../../AkAudiolib/Win32")
    list(APPEND SRC_FILES "Tremor/SIMD/AVX/floor1_avx.cpp")
else()
    set(SYSTEM_INC "../../AkAudiolib/Linux")
endif()

add_library(${PROJECT_NAME} STATIC ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${SYSTEM_INC}
    "Common"
    "Tremor"
    "../../AkAudiolib/Win32"
    "../../AkAudiolib/SoftwarePipeline"
    "../../AkAudiolib/Common"
    "../../../../include"
    "SIMD"
    "Tremor/SIMD"
    "../../Plugins/Effects/Common"
    "Tremor/FloatingPoint"
)

target_link_libraries(${PROJECT_NAME})
