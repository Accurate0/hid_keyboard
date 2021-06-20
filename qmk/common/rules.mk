UCIS_ENABLE = yes
WPM_ENABLE = yes

ifneq ("$(wildcard $(USER_PATH)/secrets.c)","")
  SRC += secrets.c
endif

ifeq ($(strip $(SECRETS)), yes)
    OPT_DEFS += -DSECRETS
endif

EXT_SRC += libc.c
EXT_SRC += calc.c
EXT_SRC += accurate0.c
