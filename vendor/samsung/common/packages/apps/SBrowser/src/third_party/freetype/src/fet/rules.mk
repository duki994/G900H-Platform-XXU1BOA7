#
# FreeType 2 Extension Samsung Font module configuration rules
#

# FET driver directory
#
FET_DIR := $(SRC_DIR)/fet


# compilation flags for the driver
#
FET_COMPILE := $(FT_COMPILE) $I$(subst /,$(COMPILER_SEP),$(FET_DIR))


# FET sources (i.e., C files)
#
FET_DRV_SRC := $(FET_DIR)/sfenhancer.c \
              $(FET_DIR)/sfreshaper.c

# FET driver headers
#

FET_DRV_H := $(FET_DRV_SRC:%c=%h)  \
               $(FET_DIR)/sftypes.h


#
#   FET_DRV_OBJ_M is used during `multi' builds.
#   FET_DRV_OBJ_S is used during `single' builds.
#
FET_DRV_OBJ_M := $(FET_DRV_SRC:$(FET_DIR)/%.c=$(OBJ_DIR)/%.$O)
FET_DRV_OBJ_S := $(OBJ_DIR)/fet.$O

# FET driver source file for single build
#
FET_DRV_SRC_S := $(FET_DIR)/fet.c


# FET driver - single object
#
$(FET_DRV_OBJ_S): $(FET_DRV_SRC_S) $(FET_DRV_SRC) \
                   $(FREETYPE_H) $(FET_DRV_H)
	$(FET_COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $(FET_DRV_SRC_S))


# FET driver - multiple objects
#
$(OBJ_DIR)/%.$O: $(FET_DIR)/%.c $(FREETYPE_H) $(FET_DRV_H)
	$(FET_COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<)


# update main driver object lists
#
DRV_OBJS_S += $(FET_DRV_OBJ_S)
DRV_OBJS_M += $(FET_DRV_OBJ_M)


# EOF
