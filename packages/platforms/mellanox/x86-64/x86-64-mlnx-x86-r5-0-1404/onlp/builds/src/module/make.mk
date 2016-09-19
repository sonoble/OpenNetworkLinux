###############################################################################
#
# 
#
###############################################################################
THIS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
x86_64_mlnx_x86_r5_0_1404_INCLUDES := -I $(THIS_DIR)inc
x86_64_mlnx_x86_r5_0_1404_INTERNAL_INCLUDES := -I $(THIS_DIR)src
x86_64_mlnx_x86_r5_0_1404_DEPENDMODULE_ENTRIES := init:x86_64_mlnx_x86_r5_0_1404 ucli:x86_64_mlnx_x86_r5_0_1404

