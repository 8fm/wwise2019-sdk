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
  PREMAKE4_BUILDTARGET_BASENAME = AkGainFX
  TARGETDIR = ../../../../../../Mac/Debug/lib
  TARGET = $(TARGETDIR)/libAkGainFX.a
  OBJDIR = ../../../../../../Mac/Debug/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -D_DEBUG
  INCLUDES += -I.. -I. -I../../../../../../include -I../../Common/Mac -I../../Common -I../../../Common
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
  PREMAKE4_BUILDTARGET_BASENAME = AkGainFX
  TARGETDIR = ../../../../../../Mac/Profile/lib
  TARGET = $(TARGETDIR)/libAkGainFX.a
  OBJDIR = ../../../../../../Mac/Profile/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -DNDEBUG
  INCLUDES += -I.. -I. -I../../../../../../include -I../../Common/Mac -I../../Common -I../../../Common
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
  PREMAKE4_BUILDTARGET_BASENAME = AkGainFX
  TARGETDIR = ../../../../../../Mac/Profile_EnableAsserts/lib
  TARGET = $(TARGETDIR)/libAkGainFX.a
  OBJDIR = ../../../../../../Mac/Profile_EnableAsserts/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -DNDEBUG -DAK_ENABLE_ASSERTS
  INCLUDES += -I.. -I. -I../../../../../../include -I../../Common/Mac -I../../Common -I../../../Common
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
  PREMAKE4_BUILDTARGET_BASENAME = AkGainFX
  TARGETDIR = ../../../../../../Mac/Release/lib
  TARGET = $(TARGETDIR)/libAkGainFX.a
  OBJDIR = ../../../../../../Mac/Release/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -DAK_OPTIMIZED -DNDEBUG
  INCLUDES += -I.. -I. -I../../../../../../include -I../../Common/Mac -I../../Common -I../../../Common
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
	$(OBJDIR)/AkGainFX.o \
	$(OBJDIR)/AkGainFXParams.o \
	$(OBJDIR)/InitAkGainFX.o \

RESOURCES := \

CUSTOMFILES := \

SHELLTYPE := posix
ifeq (sh.exe,$(SHELL))
	SHELLTYPE := msdos
endif

$(TARGET): $(GCH) ${CUSTOMFILES} $(OBJECTS) $(LDDEPS) $(RESOURCES) | $(TARGETDIR)
	@echo Linking AkGainFX
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
	@echo Cleaning AkGainFX
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

$(OBJDIR)/AkGainFX.o: ../AkGainFX.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AkGainFXParams.o: ../AkGainFXParams.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/InitAkGainFX.o: ../InitAkGainFX.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"

-include $(OBJECTS:%.o=%.d)
ifneq (,$(PCH))
  -include $(OBJDIR)/$(notdir $(PCH)).d
endif