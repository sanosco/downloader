###############################################################################
#
#  auxiliary functions
#
#  12.04.2018
#
###############################################################################

enum-same-level-subdirs = $(shell\
	for i in "$(1)"/*; do\
		if [ -d "$${i}" ]; then\
		echo "$${i}";\
		fi;\
	done)
     
enum-subdirs-in-depth = $(foreach d,$(call enum-same-level-subdirs,$(1)),$(d) $(call enum-subdirs-in-depth,$(d)))

remove-duplicates-keep-first = $(if $(1),$(firstword $(1)) $(call remove-duplicates-keep-first,$(filter-out $(firstword $(1)),$(1))))

remove-duplicates-keep-last = $(if $(1),$(call remove-duplicates-keep-last,$(filter-out $(word $(words $(1)),$(1)),$(1))) $(word $(words $(1)),$(1)))

exсlude-last = $(if $(1),$(wordlist 1,$(words $(wordlist 2,$(words $(1)),$(1))),$(1)))

remove-duplicates-libs = $(if $(1),$(strip\
	$(if $(filter -l%,$(word $(words $(1)),$(1))),\
		$(call remove-duplicates-libs,$(filter-out $(word $(words $(1)),$(1)),$(1))),\
		$(call remove-duplicates-libs,$(call exсlude-last,$(1))))\
	$(word $(words $(1)),$(1))))
