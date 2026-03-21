# This makefile only runs tests.
# By default it tests all available compilers with various options, but you can restrict the test matrix by setting the variables defined below.

# Optimization modes to test. Override this with a subset of modes if you want to.
OPTIMIZE := O0 Omax
OPTIM_FLAGS_O0_sanitized := -O0 -g -fsanitize=address -fsanitize=undefined
OPTIM_FLAGS_O0 := -O0
OPTIM_FLAGS_Omax := -O3
$(foreach x,$(OPTIMIZE),$(if $(OPTIM_FLAGS_$x),,$(error Unknown optimization level: $x)))

# Compilers to test. If not specified, we find all versions of GCC and Clang in PATH.
# Note that we only use version-suffixed compilers. The unsufixed compilers should be linked to one of those anyway?
COMPILER := $(shell bash -c 'compgen -c g++-; compgen -c clang++-; compgen -c cl; compgen -c clang-cl' | grep -Po '^((clan)?g\+\+(-[0-9]+)?|cl|clang-cl)(?=.exe)?$$' | sort -hr -t- -k2 | uniq)
$(if $(COMPILER),,$(error Unable to detect compilers, set `COMPILER=??` to a space-separated compiler list))

# C++ standards to test. Override this with a subset of standards if you want to.
# Only MSVC supports `latest`.
STANDARD := latest 26 23 20 17

# C++ standard libraries to test.
STDLIB := libstdc++ libc++ msvc

# Per-compiler flag customization. A list of `compiler=flag`. All matching flags for the current compiler are applied.
CXXFLAGS_PER_COMPILER :=

# Important compiler flags.
CXXFLAGS_DEFAULT := -Iinclude -g -pedantic-errors -Wall -Wextra -Wdeprecated -Wextra-semi -ftemplate-backtrace-limit=0
CXXFLAGS_DEFAULT_MSVC := -Iinclude -EHsc
# Less important compiler flags.
CXXFLAGS :=

SRC := tests.cpp


# Used to create local variables in a safer way. E.g. `$(call var,x := 42)`.
override var = $(eval override $(subst $,$$$$,$1))

# A line break.
override define lf :=
$(call)
$(call)
endef

.PHONY: tests
tests:
ifneq ($(words $(COMPILER)),1)
	@true $(foreach x,$(COMPILER),&& $(MAKE) --no-print-directory COMPILER=$x)
else ifneq ($(words $(STANDARD)),1)
	@true $(foreach x,$(STANDARD),&& $(MAKE) --no-print-directory STANDARD=$x)
else ifneq ($(words $(STDLIB)),1)
	@true $(foreach x,$(STDLIB),&& $(MAKE) --no-print-directory STDLIB=$x)
else ifneq ($(words $(OPTIMIZE)),1)
	@true $(foreach x,$(OPTIMIZE),&& $(MAKE) --no-print-directory OPTIMIZE=$x)
else ifneq ($(and $(filter g++%,$(COMPILER)),$(filter-out libstdc++,$(STDLIB))),)
	@true # GCC only supports libstdc++.
else ifneq ($(and $(filter clang++%,$(COMPILER)),$(filter msvc,$(STDLIB))),)
	@true # We don't use `clang` with MSVC's standard library. Instead we use `clang-cl`.
else ifneq ($(and $(filter %cl,$(COMPILER)),$(filter-out msvc,$(STDLIB))),)
	@true # MSVC only supports its own standard library.
else ifeq ($(if $(filter g++% clang++%,$(COMPILER)),$(shell $(if $(filter g++%,$(COMPILER)),$(COMPILER) -v --help,$(COMPILER) -std=c++0 -xc++ /dev/null) 2>&1 | grep 'c++$(STANDARD)'),x),)
	@true # Unsupported standard version for this compiler.
else ifneq ($(and $(filter %cl,$(COMPILER)),$(filter 26,$(STANDARD))),)
	@true # Unsupported standard version for this compiler.
else ifneq ($(and $(filter clang-cl,$(COMPILER)),$(filter 23,$(STANDARD))),)
	@true # Unsupported standard version for this compiler.
else ifneq ($(and $(filter clang++%,$(COMPILER)),$(filter libc++,$(STDLIB)),$(if $(wildcard /usr/lib/llvm-$(shell $(COMPILER) --version | grep -Po '(?<=version )[0-9]+')/include/c++),,x)),)
	@true # Using Clang with libc++, but libc++ is not installed.
else
	@+printf "%-11s C++%-7s %-10s %-14s...  " $(COMPILER) $(STANDARD) $(STDLIB) $(OPTIMIZE)
	@$(strip \
		$(if $(filter %cl,$(COMPILER)),MSYS2_ARG_CONV_EXCL=/DEBUG)\
		$(COMPILER) $(SRC) \
		$(CXXFLAGS) \
		$(CXXFLAGS_DEFAULT$(if $(filter %cl,$(COMPILER)),_MSVC)) \
		$(call var,_optim_flags := $(OPTIM_FLAGS_$(OPTIMIZE)))\
		$(if $(filter %cl,$(COMPILER)),\
    		$(call var,_optim_flags := $(filter-out -fsanitize=undefined -O0,$(_optim_flags)))\
    		$(call var,_optim_flags := $(patsubst -g,/DEBUG,$(_optim_flags)))\
    		$(call var,_optim_flags := $(patsubst -O3,-O2,$(_optim_flags)))\
		)\
		$(_optim_flags)\
		-std$(if $(filter %cl,$(COMPILER)),:,=)c++$(if $(and $(filter cl,$(COMPILER)),$(filter 23,$(STANDARD))),23preview,$(STANDARD)) \
		$(if $(filter clang++%,$(COMPILER)),-stdlib=$(STDLIB)) \
		$(patsubst $(COMPILER)=%,%,$(filter $(COMPILER)=%,$(CXXFLAGS_PER_COMPILER))) \
		$(if $(filter %cl,$(COMPILER)),-link -out:tests.exe && ./tests.exe,-otests && ./tests) \
	)
endif

.PHONY: commands
commands:
	$(eval override cxx := $(firstword $(filter clang++%,$(COMPILER))))
	$(if $(cxx),,$(error Unable to guess the compiler))
	$(eval override std := $(firstword $(filter-out latest,$(STANDARD))))
	$(if $(std),,$(error Unable to guess the C++ standard version))
	$(eval override stdlib := $(firstword $(filter-out latest,$(STDLIB))))
	$(if $(stdlib),,$(error Unable to guess the C++ standard library))
	$(file >compile_commands.json,[{"directory":"$(CURDIR)", "file":"$(abspath $(SRC))", "command":"$(cxx) $(SRC) $(CXXFLAGS) $(CXXFLAGS_DEFAULT) -std=c++$(std) -stdlib=$(stdlib)"}])
	@true


# A reminder to bump the version number.
# We store the commit hash (plus the "dirty" flag) and the current version to a file called `$(last_version_file)`.
# If the hash changes but the version doesn't, we emit an error.
# Except that when the "dirty" flag changes from true to false, we silently accept the hash change once.
CHECK_VERSION = 1# Set to 0 to disable the version number check.
ifneq ($(and $(filter 0,$(MAKELEVEL)),$(filter-out 0,$(CHECK_VERSION))),)
last_version_file := .last_version

LAST_VER :=
LAST_COMMIT :=
-include $(last_version_file)

THIS_VER = $(shell printf "%05d" $$(grep -oP '#define BETTERLISTINIT_VERSION\s+\K[0-9]+' include/better_list_init.hpp) | sed -E 's/(.*)(.{2})(.{2})/\1.\2.\3/')
$(if $(THIS_VER),,$(error Unable to determine the version number from the header.))

THIS_COMMIT := $(shell git describe --always --abbrev=0 --match "NOT A TAG" --dirty=-dirty)
$(if $(and $(THIS_COMMIT),$(filter 0,$(.SHELLSTATUS))),,$(error Unable to determine the current commit hash.))

$(info Version $(THIS_VER) at commit $(THIS_COMMIT))
$(info )

ifeq ($(LAST_VER),$(THIS_VER))
ifneq ($(LAST_COMMIT),$(THIS_COMMIT))
ifeq ($(and $(filter %-dirty,$(LAST_COMMIT)),$(filter-out %-dirty,$(THIS_COMMIT))),)
$(error Go bump the version number! Or pass `CHECK_VERSION=0` to ignore once. Or `rm .last_version` to ignore for this commit)
endif
endif
endif

$(file >$(last_version_file),LAST_VER := $(THIS_VER)$(lf)LAST_COMMIT := $(THIS_COMMIT))
endif
