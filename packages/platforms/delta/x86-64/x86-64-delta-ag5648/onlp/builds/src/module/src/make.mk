###############################################################################
#
# 
#
###############################################################################

LIBRARY := x86_64_delta_ag5648
$(LIBRARY)_SUBDIR := $(dir $(lastword $(MAKEFILE_LIST)))
include $(BUILDER)/lib.mk
