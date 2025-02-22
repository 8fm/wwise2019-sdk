# GNU Make project makefile autogenerated by Premake

ifndef config
  config=debug
endif

ifndef verbose
  SILENT = @
endif

CC = clang
CXX = clang++
AR = ar

.PHONY: clean prebuild prelink

ifeq ($(config),debug)
  PREMAKE4_BUILDTARGET_BASENAME = AkOpusDecoder
  TARGETDIR = ../../../../../Linux_$(AK_LINUX_ARCH)/Debug/lib
  TARGET = $(TARGETDIR)/libAkOpusDecoder.a
  OBJDIR = ../../../../../Linux_$(AK_LINUX_ARCH)/Debug/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -D_DEBUG -DAUDIOKINETIC -DAKSOUNDENGINE_EXPORTS -DFLOAT_APPROX -DHAVE_CONFIG_H
  INCLUDES += -I../opus -I../opus/include -I../opus/celt -I../opus/silk -I../opus/silk/float -I../../../../../include -I../../../AkAudiolib/SoftwarePipeline -I../../../AkAudiolib/Common -I../../../AkAudiolib/Linux -I../opusfile/include -I../libogg/include
  FORCE_INCLUDE +=
  ALL_CPPFLAGS += $(CPPFLAGS) -MMD -MP $(DEFINES) $(INCLUDES)
  ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -O3 -g -fPIC -fvisibility=hidden -Wno-invalid-offsetof
  ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -O3 -g -std=c++11 -fno-exceptions -fno-rtti -fPIC -fvisibility=hidden -Wno-invalid-offsetof
  ALL_RESFLAGS += $(RESFLAGS) $(DEFINES) $(INCLUDES)
  LIBS +=
  LDDEPS +=
  ALL_LDFLAGS += $(LDFLAGS)
  LINKCMD = $(AR) -rcs "$@" $(OBJECTS)
  define PREBUILDCMDS
  endef
  define PRELINKCMDS
  endef
  define POSTBUILDCMDS
  endef
all: prebuild prelink $(TARGET)
	@:

endif

ifeq ($(config),profile)
  PREMAKE4_BUILDTARGET_BASENAME = AkOpusDecoder
  TARGETDIR = ../../../../../Linux_$(AK_LINUX_ARCH)/Profile/lib
  TARGET = $(TARGETDIR)/libAkOpusDecoder.a
  OBJDIR = ../../../../../Linux_$(AK_LINUX_ARCH)/Profile/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -DNDEBUG -DAUDIOKINETIC -DAKSOUNDENGINE_EXPORTS -DFLOAT_APPROX -DHAVE_CONFIG_H
  INCLUDES += -I../opus -I../opus/include -I../opus/celt -I../opus/silk -I../opus/silk/float -I../../../../../include -I../../../AkAudiolib/SoftwarePipeline -I../../../AkAudiolib/Common -I../../../AkAudiolib/Linux -I../opusfile/include -I../libogg/include
  FORCE_INCLUDE +=
  ALL_CPPFLAGS += $(CPPFLAGS) -MMD -MP $(DEFINES) $(INCLUDES)
  ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -O3 -g -fPIC -fvisibility=hidden -Wno-invalid-offsetof
  ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -O3 -g -std=c++11 -fno-exceptions -fno-rtti -fPIC -fvisibility=hidden -Wno-invalid-offsetof
  ALL_RESFLAGS += $(RESFLAGS) $(DEFINES) $(INCLUDES)
  LIBS +=
  LDDEPS +=
  ALL_LDFLAGS += $(LDFLAGS)
  LINKCMD = $(AR) -rcs "$@" $(OBJECTS)
  define PREBUILDCMDS
  endef
  define PRELINKCMDS
  endef
  define POSTBUILDCMDS
  endef
all: prebuild prelink $(TARGET)
	@:

endif

ifeq ($(config),profile_enableasserts)
  PREMAKE4_BUILDTARGET_BASENAME = AkOpusDecoder
  TARGETDIR = ../../../../../Linux_$(AK_LINUX_ARCH)/Profile_EnableAsserts/lib
  TARGET = $(TARGETDIR)/libAkOpusDecoder.a
  OBJDIR = ../../../../../Linux_$(AK_LINUX_ARCH)/Profile_EnableAsserts/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -DNDEBUG -DAK_ENABLE_ASSERTS -DAUDIOKINETIC -DAKSOUNDENGINE_EXPORTS -DFLOAT_APPROX -DHAVE_CONFIG_H
  INCLUDES += -I../opus -I../opus/include -I../opus/celt -I../opus/silk -I../opus/silk/float -I../../../../../include -I../../../AkAudiolib/SoftwarePipeline -I../../../AkAudiolib/Common -I../../../AkAudiolib/Linux -I../opusfile/include -I../libogg/include
  FORCE_INCLUDE +=
  ALL_CPPFLAGS += $(CPPFLAGS) -MMD -MP $(DEFINES) $(INCLUDES)
  ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -O3 -g -fPIC -fvisibility=hidden -Wno-invalid-offsetof
  ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -O3 -g -std=c++11 -fno-exceptions -fno-rtti -fPIC -fvisibility=hidden -Wno-invalid-offsetof
  ALL_RESFLAGS += $(RESFLAGS) $(DEFINES) $(INCLUDES)
  LIBS +=
  LDDEPS +=
  ALL_LDFLAGS += $(LDFLAGS)
  LINKCMD = $(AR) -rcs "$@" $(OBJECTS)
  define PREBUILDCMDS
  endef
  define PRELINKCMDS
  endef
  define POSTBUILDCMDS
  endef
all: prebuild prelink $(TARGET)
	@:

endif

ifeq ($(config),release)
  PREMAKE4_BUILDTARGET_BASENAME = AkOpusDecoder
  TARGETDIR = ../../../../../Linux_$(AK_LINUX_ARCH)/Release/lib
  TARGET = $(TARGETDIR)/libAkOpusDecoder.a
  OBJDIR = ../../../../../Linux_$(AK_LINUX_ARCH)/Release/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -DAK_OPTIMIZED -DNDEBUG -DAUDIOKINETIC -DAKSOUNDENGINE_EXPORTS -DFLOAT_APPROX -DHAVE_CONFIG_H
  INCLUDES += -I../opus -I../opus/include -I../opus/celt -I../opus/silk -I../opus/silk/float -I../../../../../include -I../../../AkAudiolib/SoftwarePipeline -I../../../AkAudiolib/Common -I../../../AkAudiolib/Linux -I../opusfile/include -I../libogg/include
  FORCE_INCLUDE +=
  ALL_CPPFLAGS += $(CPPFLAGS) -MMD -MP $(DEFINES) $(INCLUDES)
  ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -O3 -g -fPIC -fvisibility=hidden -Wno-invalid-offsetof
  ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -O3 -g -std=c++11 -fno-exceptions -fno-rtti -fPIC -fvisibility=hidden -Wno-invalid-offsetof
  ALL_RESFLAGS += $(RESFLAGS) $(DEFINES) $(INCLUDES)
  LIBS +=
  LDDEPS +=
  ALL_LDFLAGS += $(LDFLAGS)
  LINKCMD = $(AR) -rcs "$@" $(OBJECTS)
  define PREBUILDCMDS
  endef
  define PRELINKCMDS
  endef
  define POSTBUILDCMDS
  endef
all: prebuild prelink $(TARGET)
	@:

endif

OBJECTS := \
	$(OBJDIR)/AkCodecWemOpus.o \
	$(OBJDIR)/AkOpusLib.o \
	$(OBJDIR)/AkSrcFileOpus.o \
	$(OBJDIR)/OpusCommon.o \
	$(OBJDIR)/bitwise.o \
	$(OBJDIR)/framing.o \
	$(OBJDIR)/SIMDCode_CELT.o \
	$(OBJDIR)/bands.o \
	$(OBJDIR)/celt.o \
	$(OBJDIR)/celt_decoder.o \
	$(OBJDIR)/celt_lpc.o \
	$(OBJDIR)/cwrs.o \
	$(OBJDIR)/entcode.o \
	$(OBJDIR)/entdec.o \
	$(OBJDIR)/entenc.o \
	$(OBJDIR)/kiss_fft.o \
	$(OBJDIR)/laplace.o \
	$(OBJDIR)/mathops.o \
	$(OBJDIR)/mdct.o \
	$(OBJDIR)/modes.o \
	$(OBJDIR)/pitch.o \
	$(OBJDIR)/quant_bands.o \
	$(OBJDIR)/rate.o \
	$(OBJDIR)/vq.o \
	$(OBJDIR)/A2NLSF.o \
	$(OBJDIR)/CNG.o \
	$(OBJDIR)/HP_variable_cutoff.o \
	$(OBJDIR)/LPC_analysis_filter.o \
	$(OBJDIR)/LPC_fit.o \
	$(OBJDIR)/LPC_inv_pred_gain.o \
	$(OBJDIR)/LP_variable_cutoff.o \
	$(OBJDIR)/NLSF2A.o \
	$(OBJDIR)/NLSF_VQ.o \
	$(OBJDIR)/NLSF_VQ_weights_laroia.o \
	$(OBJDIR)/NLSF_decode.o \
	$(OBJDIR)/NLSF_del_dec_quant.o \
	$(OBJDIR)/NLSF_encode.o \
	$(OBJDIR)/NLSF_stabilize.o \
	$(OBJDIR)/NLSF_unpack.o \
	$(OBJDIR)/NSQ.o \
	$(OBJDIR)/NSQ_del_dec.o \
	$(OBJDIR)/PLC.o \
	$(OBJDIR)/SIMDCode_SILK.o \
	$(OBJDIR)/VAD.o \
	$(OBJDIR)/VQ_WMat_EC.o \
	$(OBJDIR)/ana_filt_bank_1.o \
	$(OBJDIR)/biquad_alt.o \
	$(OBJDIR)/bwexpander.o \
	$(OBJDIR)/bwexpander_32.o \
	$(OBJDIR)/check_control_input.o \
	$(OBJDIR)/code_signs.o \
	$(OBJDIR)/control_SNR.o \
	$(OBJDIR)/control_audio_bandwidth.o \
	$(OBJDIR)/control_codec.o \
	$(OBJDIR)/debug.o \
	$(OBJDIR)/dec_API.o \
	$(OBJDIR)/decode_core.o \
	$(OBJDIR)/decode_frame.o \
	$(OBJDIR)/decode_indices.o \
	$(OBJDIR)/decode_parameters.o \
	$(OBJDIR)/decode_pitch.o \
	$(OBJDIR)/decode_pulses.o \
	$(OBJDIR)/decoder_set_fs.o \
	$(OBJDIR)/LPC_analysis_filter_FLP.o \
	$(OBJDIR)/LPC_inv_pred_gain_FLP.o \
	$(OBJDIR)/LTP_analysis_filter_FLP.o \
	$(OBJDIR)/LTP_scale_ctrl_FLP.o \
	$(OBJDIR)/apply_sine_window_FLP.o \
	$(OBJDIR)/autocorrelation_FLP.o \
	$(OBJDIR)/burg_modified_FLP.o \
	$(OBJDIR)/bwexpander_FLP.o \
	$(OBJDIR)/corrMatrix_FLP.o \
	$(OBJDIR)/encode_frame_FLP.o \
	$(OBJDIR)/energy_FLP.o \
	$(OBJDIR)/find_LPC_FLP.o \
	$(OBJDIR)/find_LTP_FLP.o \
	$(OBJDIR)/find_pitch_lags_FLP.o \
	$(OBJDIR)/find_pred_coefs_FLP.o \
	$(OBJDIR)/inner_product_FLP.o \
	$(OBJDIR)/k2a_FLP.o \
	$(OBJDIR)/noise_shape_analysis_FLP.o \
	$(OBJDIR)/pitch_analysis_core_FLP.o \
	$(OBJDIR)/process_gains_FLP.o \
	$(OBJDIR)/regularize_correlations_FLP.o \
	$(OBJDIR)/residual_energy_FLP.o \
	$(OBJDIR)/scale_copy_vector_FLP.o \
	$(OBJDIR)/scale_vector_FLP.o \
	$(OBJDIR)/schur_FLP.o \
	$(OBJDIR)/sort_FLP.o \
	$(OBJDIR)/warped_autocorrelation_FLP.o \
	$(OBJDIR)/wrappers_FLP.o \
	$(OBJDIR)/gain_quant.o \
	$(OBJDIR)/init_decoder.o \
	$(OBJDIR)/inner_prod_aligned.o \
	$(OBJDIR)/interpolate.o \
	$(OBJDIR)/lin2log.o \
	$(OBJDIR)/log2lin.o \
	$(OBJDIR)/pitch_est_tables.o \
	$(OBJDIR)/process_NLSFs.o \
	$(OBJDIR)/quant_LTP_gains.o \
	$(OBJDIR)/resampler.o \
	$(OBJDIR)/resampler_down2.o \
	$(OBJDIR)/resampler_down2_3.o \
	$(OBJDIR)/resampler_private_AR2.o \
	$(OBJDIR)/resampler_private_IIR_FIR.o \
	$(OBJDIR)/resampler_private_down_FIR.o \
	$(OBJDIR)/resampler_private_up2_HQ.o \
	$(OBJDIR)/resampler_rom.o \
	$(OBJDIR)/shell_coder.o \
	$(OBJDIR)/sigm_Q15.o \
	$(OBJDIR)/sort.o \
	$(OBJDIR)/stereo_LR_to_MS.o \
	$(OBJDIR)/stereo_MS_to_LR.o \
	$(OBJDIR)/stereo_decode_pred.o \
	$(OBJDIR)/stereo_encode_pred.o \
	$(OBJDIR)/stereo_find_predictor.o \
	$(OBJDIR)/stereo_quant_pred.o \
	$(OBJDIR)/sum_sqr_shift.o \
	$(OBJDIR)/table_LSF_cos.o \
	$(OBJDIR)/tables_LTP.o \
	$(OBJDIR)/tables_NLSF_CB_NB_MB.o \
	$(OBJDIR)/tables_NLSF_CB_WB.o \
	$(OBJDIR)/tables_gain.o \
	$(OBJDIR)/tables_other.o \
	$(OBJDIR)/tables_pitch_lag.o \
	$(OBJDIR)/tables_pulses_per_block.o \
	$(OBJDIR)/opus.o \
	$(OBJDIR)/opus_decoder.o \
	$(OBJDIR)/opus_multistream.o \
	$(OBJDIR)/opus_multistream_decoder.o \
	$(OBJDIR)/http.o \
	$(OBJDIR)/info.o \
	$(OBJDIR)/internal.o \
	$(OBJDIR)/opusfile.o \
	$(OBJDIR)/stream.o \
	$(OBJDIR)/wincerts.o \

RESOURCES := \

CUSTOMFILES := \

SHELLTYPE := posix
ifeq (sh.exe,$(SHELL))
	SHELLTYPE := msdos
endif

$(TARGET): $(GCH) ${CUSTOMFILES} $(OBJECTS) $(LDDEPS) $(RESOURCES) | $(TARGETDIR)
	@echo Linking AkOpusDecoder
	$(SILENT) $(LINKCMD)
	$(POSTBUILDCMDS)

$(CUSTOMFILES): | $(OBJDIR)

$(TARGETDIR):
	@echo Creating $(TARGETDIR)
ifeq (posix,$(SHELLTYPE))
	$(SILENT) mkdir -p $(TARGETDIR)
else
	$(SILENT) if not exist $(subst /,\\,$(TARGETDIR)) mkdir $(subst /,\\,$(TARGETDIR))
endif

$(OBJDIR):
	@echo Creating $(OBJDIR)
ifeq (posix,$(SHELLTYPE))
	$(SILENT) mkdir -p $(OBJDIR)
else
	$(SILENT) if not exist $(subst /,\\,$(OBJDIR)) mkdir $(subst /,\\,$(OBJDIR))
endif

clean:
	@echo Cleaning AkOpusDecoder
ifeq (posix,$(SHELLTYPE))
	$(SILENT) rm -f  $(TARGET)
	$(SILENT) rm -rf $(OBJDIR)
else
	$(SILENT) if exist $(subst /,\\,$(TARGET)) del $(subst /,\\,$(TARGET))
	$(SILENT) if exist $(subst /,\\,$(OBJDIR)) rmdir /s /q $(subst /,\\,$(OBJDIR))
endif

prebuild:
	$(PREBUILDCMDS)

prelink:
	$(PRELINKCMDS)

ifneq (,$(PCH))
$(OBJECTS): $(GCH) $(PCH) | $(OBJDIR)
$(GCH): $(PCH) | $(OBJDIR)
	@echo $(notdir $<)
	$(SILENT) $(CXX) -x c++-header $(ALL_CXXFLAGS) -o "$@" -MF "$(@:%.gch=%.d)" -c "$<"
else
$(OBJECTS): | $(OBJDIR)
endif

$(OBJDIR)/AkCodecWemOpus.o: ../AkCodecWemOpus.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AkOpusLib.o: ../AkOpusLib.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AkSrcFileOpus.o: ../AkSrcFileOpus.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/OpusCommon.o: ../OpusCommon.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/bitwise.o: ../libogg/src/bitwise.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/framing.o: ../libogg/src/framing.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/SIMDCode_CELT.o: ../opus/celt/SIMDCode_CELT.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/bands.o: ../opus/celt/bands.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/celt.o: ../opus/celt/celt.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/celt_decoder.o: ../opus/celt/celt_decoder.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/celt_lpc.o: ../opus/celt/celt_lpc.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/cwrs.o: ../opus/celt/cwrs.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/entcode.o: ../opus/celt/entcode.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/entdec.o: ../opus/celt/entdec.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/entenc.o: ../opus/celt/entenc.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/kiss_fft.o: ../opus/celt/kiss_fft.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/laplace.o: ../opus/celt/laplace.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/mathops.o: ../opus/celt/mathops.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/mdct.o: ../opus/celt/mdct.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/modes.o: ../opus/celt/modes.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/pitch.o: ../opus/celt/pitch.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/quant_bands.o: ../opus/celt/quant_bands.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/rate.o: ../opus/celt/rate.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/vq.o: ../opus/celt/vq.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/A2NLSF.o: ../opus/silk/A2NLSF.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/CNG.o: ../opus/silk/CNG.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/HP_variable_cutoff.o: ../opus/silk/HP_variable_cutoff.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/LPC_analysis_filter.o: ../opus/silk/LPC_analysis_filter.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/LPC_fit.o: ../opus/silk/LPC_fit.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/LPC_inv_pred_gain.o: ../opus/silk/LPC_inv_pred_gain.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/LP_variable_cutoff.o: ../opus/silk/LP_variable_cutoff.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/NLSF2A.o: ../opus/silk/NLSF2A.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/NLSF_VQ.o: ../opus/silk/NLSF_VQ.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/NLSF_VQ_weights_laroia.o: ../opus/silk/NLSF_VQ_weights_laroia.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/NLSF_decode.o: ../opus/silk/NLSF_decode.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/NLSF_del_dec_quant.o: ../opus/silk/NLSF_del_dec_quant.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/NLSF_encode.o: ../opus/silk/NLSF_encode.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/NLSF_stabilize.o: ../opus/silk/NLSF_stabilize.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/NLSF_unpack.o: ../opus/silk/NLSF_unpack.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/NSQ.o: ../opus/silk/NSQ.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/NSQ_del_dec.o: ../opus/silk/NSQ_del_dec.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/PLC.o: ../opus/silk/PLC.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/SIMDCode_SILK.o: ../opus/silk/SIMDCode_SILK.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/VAD.o: ../opus/silk/VAD.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/VQ_WMat_EC.o: ../opus/silk/VQ_WMat_EC.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/ana_filt_bank_1.o: ../opus/silk/ana_filt_bank_1.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/biquad_alt.o: ../opus/silk/biquad_alt.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/bwexpander.o: ../opus/silk/bwexpander.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/bwexpander_32.o: ../opus/silk/bwexpander_32.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/check_control_input.o: ../opus/silk/check_control_input.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/code_signs.o: ../opus/silk/code_signs.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/control_SNR.o: ../opus/silk/control_SNR.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/control_audio_bandwidth.o: ../opus/silk/control_audio_bandwidth.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/control_codec.o: ../opus/silk/control_codec.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/debug.o: ../opus/silk/debug.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/dec_API.o: ../opus/silk/dec_API.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/decode_core.o: ../opus/silk/decode_core.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/decode_frame.o: ../opus/silk/decode_frame.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/decode_indices.o: ../opus/silk/decode_indices.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/decode_parameters.o: ../opus/silk/decode_parameters.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/decode_pitch.o: ../opus/silk/decode_pitch.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/decode_pulses.o: ../opus/silk/decode_pulses.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/decoder_set_fs.o: ../opus/silk/decoder_set_fs.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/LPC_analysis_filter_FLP.o: ../opus/silk/float/LPC_analysis_filter_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/LPC_inv_pred_gain_FLP.o: ../opus/silk/float/LPC_inv_pred_gain_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/LTP_analysis_filter_FLP.o: ../opus/silk/float/LTP_analysis_filter_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/LTP_scale_ctrl_FLP.o: ../opus/silk/float/LTP_scale_ctrl_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/apply_sine_window_FLP.o: ../opus/silk/float/apply_sine_window_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/autocorrelation_FLP.o: ../opus/silk/float/autocorrelation_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/burg_modified_FLP.o: ../opus/silk/float/burg_modified_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/bwexpander_FLP.o: ../opus/silk/float/bwexpander_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/corrMatrix_FLP.o: ../opus/silk/float/corrMatrix_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/encode_frame_FLP.o: ../opus/silk/float/encode_frame_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/energy_FLP.o: ../opus/silk/float/energy_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/find_LPC_FLP.o: ../opus/silk/float/find_LPC_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/find_LTP_FLP.o: ../opus/silk/float/find_LTP_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/find_pitch_lags_FLP.o: ../opus/silk/float/find_pitch_lags_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/find_pred_coefs_FLP.o: ../opus/silk/float/find_pred_coefs_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/inner_product_FLP.o: ../opus/silk/float/inner_product_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/k2a_FLP.o: ../opus/silk/float/k2a_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/noise_shape_analysis_FLP.o: ../opus/silk/float/noise_shape_analysis_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/pitch_analysis_core_FLP.o: ../opus/silk/float/pitch_analysis_core_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/process_gains_FLP.o: ../opus/silk/float/process_gains_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/regularize_correlations_FLP.o: ../opus/silk/float/regularize_correlations_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/residual_energy_FLP.o: ../opus/silk/float/residual_energy_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/scale_copy_vector_FLP.o: ../opus/silk/float/scale_copy_vector_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/scale_vector_FLP.o: ../opus/silk/float/scale_vector_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/schur_FLP.o: ../opus/silk/float/schur_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/sort_FLP.o: ../opus/silk/float/sort_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/warped_autocorrelation_FLP.o: ../opus/silk/float/warped_autocorrelation_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/wrappers_FLP.o: ../opus/silk/float/wrappers_FLP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/gain_quant.o: ../opus/silk/gain_quant.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/init_decoder.o: ../opus/silk/init_decoder.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/inner_prod_aligned.o: ../opus/silk/inner_prod_aligned.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/interpolate.o: ../opus/silk/interpolate.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/lin2log.o: ../opus/silk/lin2log.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/log2lin.o: ../opus/silk/log2lin.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/pitch_est_tables.o: ../opus/silk/pitch_est_tables.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/process_NLSFs.o: ../opus/silk/process_NLSFs.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/quant_LTP_gains.o: ../opus/silk/quant_LTP_gains.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/resampler.o: ../opus/silk/resampler.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/resampler_down2.o: ../opus/silk/resampler_down2.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/resampler_down2_3.o: ../opus/silk/resampler_down2_3.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/resampler_private_AR2.o: ../opus/silk/resampler_private_AR2.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/resampler_private_IIR_FIR.o: ../opus/silk/resampler_private_IIR_FIR.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/resampler_private_down_FIR.o: ../opus/silk/resampler_private_down_FIR.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/resampler_private_up2_HQ.o: ../opus/silk/resampler_private_up2_HQ.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/resampler_rom.o: ../opus/silk/resampler_rom.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/shell_coder.o: ../opus/silk/shell_coder.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/sigm_Q15.o: ../opus/silk/sigm_Q15.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/sort.o: ../opus/silk/sort.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/stereo_LR_to_MS.o: ../opus/silk/stereo_LR_to_MS.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/stereo_MS_to_LR.o: ../opus/silk/stereo_MS_to_LR.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/stereo_decode_pred.o: ../opus/silk/stereo_decode_pred.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/stereo_encode_pred.o: ../opus/silk/stereo_encode_pred.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/stereo_find_predictor.o: ../opus/silk/stereo_find_predictor.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/stereo_quant_pred.o: ../opus/silk/stereo_quant_pred.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/sum_sqr_shift.o: ../opus/silk/sum_sqr_shift.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/table_LSF_cos.o: ../opus/silk/table_LSF_cos.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/tables_LTP.o: ../opus/silk/tables_LTP.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/tables_NLSF_CB_NB_MB.o: ../opus/silk/tables_NLSF_CB_NB_MB.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/tables_NLSF_CB_WB.o: ../opus/silk/tables_NLSF_CB_WB.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/tables_gain.o: ../opus/silk/tables_gain.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/tables_other.o: ../opus/silk/tables_other.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/tables_pitch_lag.o: ../opus/silk/tables_pitch_lag.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/tables_pulses_per_block.o: ../opus/silk/tables_pulses_per_block.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/opus.o: ../opus/src/opus.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/opus_decoder.o: ../opus/src/opus_decoder.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/opus_multistream.o: ../opus/src/opus_multistream.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/opus_multistream_decoder.o: ../opus/src/opus_multistream_decoder.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/http.o: ../opusfile/src/http.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/info.o: ../opusfile/src/info.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/internal.o: ../opusfile/src/internal.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/opusfile.o: ../opusfile/src/opusfile.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/stream.o: ../opusfile/src/stream.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/wincerts.o: ../opusfile/src/wincerts.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"

-include $(OBJECTS:%.o=%.d)
ifneq (,$(PCH))
  -include $(OBJDIR)/$(notdir $(PCH)).d
endif