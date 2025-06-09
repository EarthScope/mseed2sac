# List of subdirectories to build
SUBDIRS = libmseed src

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)
clean: $(SUBDIRS)

# src depends on libmseed being built first
src: libmseed

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: install
install:
	@echo
	@echo "No install method"
	@echo "Copy the binary and documentation to desired location"
	@echo
