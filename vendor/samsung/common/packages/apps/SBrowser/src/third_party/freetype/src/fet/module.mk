#
# FreeType 2 Extension Samsung Font module definition
#


FTMODULE_H_COMMANDS += FET_DRIVER

define FET_DRIVER
$(OPEN_DRIVER) FT_Driver_ClassRec, fet_driver_class $(CLOSE_DRIVER)
$(ECHO_DRIVER)fet  $(ECHO_DRIVER_DESC)font reshaper and enhancer driver$(ECHO_DRIVER_DONE)
endef

# EOF
