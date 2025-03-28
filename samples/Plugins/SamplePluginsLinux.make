# GNU Make workspace makefile autogenerated by Premake

ifndef config
  config=debug
endif

ifndef verbose
  SILENT = @
endif

ifeq ($(config),debug)
  AkAudioInputSource_config = debug
  AkDelayFX_config = debug
  AkSineSource_config = debug
  AkToneSource_config = debug
endif
ifeq ($(config),profile)
  AkAudioInputSource_config = profile
  AkDelayFX_config = profile
  AkSineSource_config = profile
  AkToneSource_config = profile
endif
ifeq ($(config),profile_enableasserts)
  AkAudioInputSource_config = profile_enableasserts
  AkDelayFX_config = profile_enableasserts
  AkSineSource_config = profile_enableasserts
  AkToneSource_config = profile_enableasserts
endif
ifeq ($(config),release)
  AkAudioInputSource_config = release
  AkDelayFX_config = release
  AkSineSource_config = release
  AkToneSource_config = release
endif

PROJECTS := AkAudioInputSource AkDelayFX AkSineSource AkToneSource

.PHONY: all clean help $(PROJECTS) Plugins Plugins/Effects Plugins/Sources

all: $(PROJECTS)

Plugins: Plugins/Effects Plugins/Sources

Plugins/Effects: AkDelayFX

Plugins/Sources: AkAudioInputSource AkSineSource AkToneSource

AkAudioInputSource:
	@echo "==== Building AkAudioInputSource ($(AkAudioInputSource_config)) ===="
	@${MAKE} --no-print-directory -C AkAudioInput/Sources/AudioEngineFX/Linux -f AkAudioInputSourceLinux.make config=$(AkAudioInputSource_config)

AkDelayFX:
	@echo "==== Building AkDelayFX ($(AkDelayFX_config)) ===="
	@${MAKE} --no-print-directory -C AkDelay/Sources/AudioEngineFX/Linux -f AkDelayFXLinux.make config=$(AkDelayFX_config)

AkSineSource:
	@echo "==== Building AkSineSource ($(AkSineSource_config)) ===="
	@${MAKE} --no-print-directory -C AkSineTone/Sources/AudioEngineFX/Linux -f AkSineSourceLinux.make config=$(AkSineSource_config)

AkToneSource:
	@echo "==== Building AkToneSource ($(AkToneSource_config)) ===="
	@${MAKE} --no-print-directory -C AkToneGenerator/Sources/AudioEngineFX/Linux -f AkToneSourceLinux.make config=$(AkToneSource_config)

clean:
	@${MAKE} --no-print-directory -C AkAudioInput/Sources/AudioEngineFX/Linux -f AkAudioInputSourceLinux.make clean
	@${MAKE} --no-print-directory -C AkDelay/Sources/AudioEngineFX/Linux -f AkDelayFXLinux.make clean
	@${MAKE} --no-print-directory -C AkSineTone/Sources/AudioEngineFX/Linux -f AkSineSourceLinux.make clean
	@${MAKE} --no-print-directory -C AkToneGenerator/Sources/AudioEngineFX/Linux -f AkToneSourceLinux.make clean

help:
	@echo "Usage: make [config=name] [target]"
	@echo ""
	@echo "CONFIGURATIONS:"
	@echo "  debug"
	@echo "  profile"
	@echo "  profile_enableasserts"
	@echo "  release"
	@echo ""
	@echo "TARGETS:"
	@echo "   all (default)"
	@echo "   clean"
	@echo "   AkAudioInputSource"
	@echo "   AkDelayFX"
	@echo "   AkSineSource"
	@echo "   AkToneSource"
	@echo ""
	@echo "For more information, see https://github.com/premake/premake-core/wiki"