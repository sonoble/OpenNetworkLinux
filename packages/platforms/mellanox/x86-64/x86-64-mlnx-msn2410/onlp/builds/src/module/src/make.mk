###############################################################################
#
# 
#
###############################################################################

LIBRARY := x86_64_mlnx_msn2410
$(LIBRARY)_SUBDIR := $(dir $(lastword $(MAKEFILE_LIST)))
include $(BUILDER)/lib.mk
