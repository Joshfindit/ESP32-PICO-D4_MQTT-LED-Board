#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#
PROJECT_NAME := mqtt_led
EXTRA_COMPONENT_DIRS += $(PROJECT_PATH)/../../../

include $(IDF_PATH)/make/project.mk

