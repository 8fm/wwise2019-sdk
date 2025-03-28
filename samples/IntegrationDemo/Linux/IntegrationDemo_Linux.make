# GNU Make workspace makefile autogenerated by Premake

ifndef config
  config=debug
endif

ifndef verbose
  SILENT = @
endif

ifeq ($(config),debug)
  IntegrationDemo_config = debug
endif
ifeq ($(config),profile)
  IntegrationDemo_config = profile
endif
ifeq ($(config),release)
  IntegrationDemo_config = release
endif

PROJECTS := IntegrationDemo

.PHONY: all clean help $(PROJECTS) 

all: $(PROJECTS)

IntegrationDemo:
	@echo "==== Building IntegrationDemo ($(IntegrationDemo_config)) ===="
	@${MAKE} --no-print-directory -C . -f IntegrationDemo.make config=$(IntegrationDemo_config)

clean:
	@${MAKE} --no-print-directory -C . -f IntegrationDemo.make clean

help:
	@echo "Usage: make [config=name] [target]"
	@echo ""
	@echo "CONFIGURATIONS:"
	@echo "  debug"
	@echo "  profile"
	@echo "  release"
	@echo ""
	@echo "TARGETS:"
	@echo "   all (default)"
	@echo "   clean"
	@echo "   IntegrationDemo"
	@echo ""
	@echo "For more information, see https://github.com/premake/premake-core/wiki"