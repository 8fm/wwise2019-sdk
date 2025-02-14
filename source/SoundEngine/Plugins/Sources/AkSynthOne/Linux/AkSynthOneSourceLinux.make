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
  PREMAKE4_BUILDTARGET_BASENAME = AkSynthOneSource
  TARGETDIR = ../../../../../../Linux_$(AK_LINUX_ARCH)/Debug/lib
  TARGET = $(TARGETDIR)/libAkSynthOneSource.a
  OBJDIR = ../../../../../../Linux_$(AK_LINUX_ARCH)/Debug/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -DAKSOUNDENGINE_EXPORTS -D_DEBUG
  INCLUDES += -I.. -I. -I../../../../../../include -I../../Common/Linux -I../../Common -I../../../Common -I../../../Effects/Common -I../../../../Common -I../../../../AkAudiolib/Common
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
  PREMAKE4_BUILDTARGET_BASENAME = AkSynthOneSource
  TARGETDIR = ../../../../../../Linux_$(AK_LINUX_ARCH)/Profile/lib
  TARGET = $(TARGETDIR)/libAkSynthOneSource.a
  OBJDIR = ../../../../../../Linux_$(AK_LINUX_ARCH)/Profile/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -DAKSOUNDENGINE_EXPORTS -DNDEBUG
  INCLUDES += -I.. -I. -I../../../../../../include -I../../Common/Linux -I../../Common -I../../../Common -I../../../Effects/Common -I../../../../Common -I../../../../AkAudiolib/Common
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
  PREMAKE4_BUILDTARGET_BASENAME = AkSynthOneSource
  TARGETDIR = ../../../../../../Linux_$(AK_LINUX_ARCH)/Profile_EnableAsserts/lib
  TARGET = $(TARGETDIR)/libAkSynthOneSource.a
  OBJDIR = ../../../../../../Linux_$(AK_LINUX_ARCH)/Profile_EnableAsserts/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -DAKSOUNDENGINE_EXPORTS -DNDEBUG -DAK_ENABLE_ASSERTS
  INCLUDES += -I.. -I. -I../../../../../../include -I../../Common/Linux -I../../Common -I../../../Common -I../../../Effects/Common -I../../../../Common -I../../../../AkAudiolib/Common
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
  PREMAKE4_BUILDTARGET_BASENAME = AkSynthOneSource
  TARGETDIR = ../../../../../../Linux_$(AK_LINUX_ARCH)/Release/lib
  TARGET = $(TARGETDIR)/libAkSynthOneSource.a
  OBJDIR = ../../../../../../Linux_$(AK_LINUX_ARCH)/Release/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -DAKSOUNDENGINE_EXPORTS -DAK_OPTIMIZED -DNDEBUG
  INCLUDES += -I.. -I. -I../../../../../../include -I../../Common/Linux -I../../Common -I../../../Common -I../../../Effects/Common -I../../../../Common -I../../../../AkAudiolib/Common
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
	$(OBJDIR)/AkColoredNoise.o \
	$(OBJDIR)/AkWhiteNoise.o \
	$(OBJDIR)/OnePoleZeroHPFilter.o \
	$(OBJDIR)/AkSynthOne.o \
	$(OBJDIR)/AkSynthOneDsp.o \
	$(OBJDIR)/AkSynthOneParams.o \
	$(OBJDIR)/InitAkSynthOne.o \

RESOURCES := \

CUSTOMFILES := \

SHELLTYPE := posix
ifeq (sh.exe,$(SHELL))
	SHELLTYPE := msdos
endif

$(TARGET): $(GCH) ${CUSTOMFILES} $(OBJECTS) $(LDDEPS) $(RESOURCES) | $(TARGETDIR)
	@echo Linking AkSynthOneSource
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
	@echo Cleaning AkSynthOneSource
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

$(OBJDIR)/AkColoredNoise.o: ../../../Common/AkColoredNoise.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AkWhiteNoise.o: ../../../Common/AkWhiteNoise.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/OnePoleZeroHPFilter.o: ../../../Effects/Common/OnePoleZeroHPFilter.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AkSynthOne.o: ../AkSynthOne.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AkSynthOneDsp.o: ../AkSynthOneDsp.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AkSynthOneParams.o: ../AkSynthOneParams.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/InitAkSynthOne.o: ../InitAkSynthOne.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"

-include $(OBJECTS:%.o=%.d)
ifneq (,$(PCH))
  -include $(OBJDIR)/$(notdir $(PCH)).d
endif