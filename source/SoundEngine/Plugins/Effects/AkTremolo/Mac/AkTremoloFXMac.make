# GNU Make project makefile autogenerated by Premake

ifndef config
  config=debug_macos
endif

ifndef verbose
  SILENT = @
endif

CC = clang
CXX = clang++
AR = ar

.PHONY: clean prebuild prelink

ifeq ($(config),debug_macos)
  PREMAKE4_BUILDTARGET_BASENAME = AkTremoloFX
  TARGETDIR = ../../../../../../Mac/Debug/lib
  TARGET = $(TARGETDIR)/libAkTremoloFX.a
  OBJDIR = ../../../../../../Mac/Debug/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -D_DEBUG
  INCLUDES += -I.. -I. -I../../../../../../include -I../../Common/Mac -I../../Common -I../../../Common -I../../../../AkAudiolib/Common
  FORCE_INCLUDE +=
  ALL_CPPFLAGS += $(CPPFLAGS) -MMD -MP $(DEFINES) $(INCLUDES)
  ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -O3 -g
  ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -O3 -g -std=c++11
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

ifeq ($(config),profile_macos)
  PREMAKE4_BUILDTARGET_BASENAME = AkTremoloFX
  TARGETDIR = ../../../../../../Mac/Profile/lib
  TARGET = $(TARGETDIR)/libAkTremoloFX.a
  OBJDIR = ../../../../../../Mac/Profile/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -DNDEBUG
  INCLUDES += -I.. -I. -I../../../../../../include -I../../Common/Mac -I../../Common -I../../../Common -I../../../../AkAudiolib/Common
  FORCE_INCLUDE +=
  ALL_CPPFLAGS += $(CPPFLAGS) -MMD -MP $(DEFINES) $(INCLUDES)
  ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -O3 -g
  ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -O3 -g -std=c++11
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

ifeq ($(config),profile_enableasserts_macos)
  PREMAKE4_BUILDTARGET_BASENAME = AkTremoloFX
  TARGETDIR = ../../../../../../Mac/Profile_EnableAsserts/lib
  TARGET = $(TARGETDIR)/libAkTremoloFX.a
  OBJDIR = ../../../../../../Mac/Profile_EnableAsserts/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -DNDEBUG -DAK_ENABLE_ASSERTS
  INCLUDES += -I.. -I. -I../../../../../../include -I../../Common/Mac -I../../Common -I../../../Common -I../../../../AkAudiolib/Common
  FORCE_INCLUDE +=
  ALL_CPPFLAGS += $(CPPFLAGS) -MMD -MP $(DEFINES) $(INCLUDES)
  ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -O3 -g
  ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -O3 -g -std=c++11
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

ifeq ($(config),release_macos)
  PREMAKE4_BUILDTARGET_BASENAME = AkTremoloFX
  TARGETDIR = ../../../../../../Mac/Release/lib
  TARGET = $(TARGETDIR)/libAkTremoloFX.a
  OBJDIR = ../../../../../../Mac/Release/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -DAK_OPTIMIZED -DNDEBUG
  INCLUDES += -I.. -I. -I../../../../../../include -I../../Common/Mac -I../../Common -I../../../Common -I../../../../AkAudiolib/Common
  FORCE_INCLUDE +=
  ALL_CPPFLAGS += $(CPPFLAGS) -MMD -MP $(DEFINES) $(INCLUDES)
  ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -O3 -g
  ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -O3 -g -std=c++11
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
	$(OBJDIR)/AkTremoloFX.o \
	$(OBJDIR)/AkTremoloFXParams.o \
	$(OBJDIR)/LFO.o \
	$(OBJDIR)/InitAkTremoloFX.o \

RESOURCES := \

CUSTOMFILES := \

SHELLTYPE := posix
ifeq (sh.exe,$(SHELL))
	SHELLTYPE := msdos
endif

$(TARGET): $(GCH) ${CUSTOMFILES} $(OBJECTS) $(LDDEPS) $(RESOURCES) | $(TARGETDIR)
	@echo Linking AkTremoloFX
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
	@echo Cleaning AkTremoloFX
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

$(OBJDIR)/AkTremoloFX.o: ../AkTremoloFX.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AkTremoloFXParams.o: ../AkTremoloFXParams.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/LFO.o: ../../Common/LFO.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/InitAkTremoloFX.o: ../InitAkTremoloFX.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"

-include $(OBJECTS:%.o=%.d)
ifneq (,$(PCH))
  -include $(OBJDIR)/$(notdir $(PCH)).d
endif