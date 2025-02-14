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
  PREMAKE4_BUILDTARGET_BASENAME = AkSoundSeedGrain
  TARGETDIR = ../../../../Linux_$(AK_LINUX_ARCH)/Debug/bin
  TARGET = $(TARGETDIR)/libAkSoundSeedGrain.so
  OBJDIR = ../../../../Linux_$(AK_LINUX_ARCH)/Debug/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -D_DEBUG
  INCLUDES += -I../../../../include
  FORCE_INCLUDE +=
  ALL_CPPFLAGS += $(CPPFLAGS) -MMD -MP $(DEFINES) $(INCLUDES)
  ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -fPIC -g -fPIC -fvisibility=hidden -Wno-invalid-offsetof
  ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -fPIC -g -std=c++11 -fno-exceptions -fno-rtti -fPIC -fvisibility=hidden -Wno-invalid-offsetof
  ALL_RESFLAGS += $(RESFLAGS) $(DEFINES) $(INCLUDES)
  LIBS += -lAkSoundSeedGrainSource
  LDDEPS +=
  ALL_LDFLAGS += $(LDFLAGS) -L"../../../../Linux_$(AK_LINUX_ARCH)/Debug/lib" -shared -Wl,-soname=libAkSoundSeedGrain.so -Wl,--export-dynamic
  LINKCMD = $(CXX) -o "$@" $(OBJECTS) $(RESOURCES) $(ALL_LDFLAGS) $(LIBS)
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
  PREMAKE4_BUILDTARGET_BASENAME = AkSoundSeedGrain
  TARGETDIR = ../../../../Linux_$(AK_LINUX_ARCH)/Profile/bin
  TARGET = $(TARGETDIR)/libAkSoundSeedGrain.so
  OBJDIR = ../../../../Linux_$(AK_LINUX_ARCH)/Profile/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -DNDEBUG
  INCLUDES += -I../../../../include
  FORCE_INCLUDE +=
  ALL_CPPFLAGS += $(CPPFLAGS) -MMD -MP $(DEFINES) $(INCLUDES)
  ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -O3 -fPIC -g -fPIC -fvisibility=hidden -Wno-invalid-offsetof
  ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -O3 -fPIC -g -std=c++11 -fno-exceptions -fno-rtti -fPIC -fvisibility=hidden -Wno-invalid-offsetof
  ALL_RESFLAGS += $(RESFLAGS) $(DEFINES) $(INCLUDES)
  LIBS += -lAkSoundSeedGrainSource
  LDDEPS +=
  ALL_LDFLAGS += $(LDFLAGS) -L"../../../../Linux_$(AK_LINUX_ARCH)/Profile/lib" -shared -Wl,-soname=libAkSoundSeedGrain.so -Wl,--export-dynamic
  LINKCMD = $(CXX) -o "$@" $(OBJECTS) $(RESOURCES) $(ALL_LDFLAGS) $(LIBS)
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
  PREMAKE4_BUILDTARGET_BASENAME = AkSoundSeedGrain
  TARGETDIR = ../../../../Linux_$(AK_LINUX_ARCH)/Release/bin
  TARGET = $(TARGETDIR)/libAkSoundSeedGrain.so
  OBJDIR = ../../../../Linux_$(AK_LINUX_ARCH)/Release/obj/$(PREMAKE4_BUILDTARGET_BASENAME)
  DEFINES += -DAK_OPTIMIZED -DNDEBUG
  INCLUDES += -I../../../../include
  FORCE_INCLUDE +=
  ALL_CPPFLAGS += $(CPPFLAGS) -MMD -MP $(DEFINES) $(INCLUDES)
  ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -O3 -fPIC -g -fPIC -fvisibility=hidden -Wno-invalid-offsetof
  ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -O3 -fPIC -g -std=c++11 -fno-exceptions -fno-rtti -fPIC -fvisibility=hidden -Wno-invalid-offsetof
  ALL_RESFLAGS += $(RESFLAGS) $(DEFINES) $(INCLUDES)
  LIBS += -lAkSoundSeedGrainSource
  LDDEPS +=
  ALL_LDFLAGS += $(LDFLAGS) -L"../../../../Linux_$(AK_LINUX_ARCH)/Release/lib" -shared -Wl,-soname=libAkSoundSeedGrain.so -Wl,--export-dynamic
  LINKCMD = $(CXX) -o "$@" $(OBJECTS) $(RESOURCES) $(ALL_LDFLAGS) $(LIBS)
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
	$(OBJDIR)/InitPlugins.o \

RESOURCES := \

CUSTOMFILES := \

SHELLTYPE := posix
ifeq (sh.exe,$(SHELL))
	SHELLTYPE := msdos
endif

$(TARGET): $(GCH) ${CUSTOMFILES} $(OBJECTS) $(LDDEPS) $(RESOURCES) | $(TARGETDIR)
	@echo Linking AkSoundSeedGrain
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
	@echo Cleaning AkSoundSeedGrain
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

$(OBJDIR)/InitPlugins.o: ../InitPlugins.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"

-include $(OBJECTS:%.o=%.d)
ifneq (,$(PCH))
  -include $(OBJDIR)/$(notdir $(PCH)).d
endif