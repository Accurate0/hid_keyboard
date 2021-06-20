UCIS_ENABLE = yes
WPM_ENABLE = yes

ifneq ("$(wildcard $(USER_PATH)/secrets.c)","")
  SRC += secrets.c
endif

ifeq ($(strip $(SECRETS)), yes)
    OPT_DEFS += -DSECRETS
endif

SRC += libc.c
SRC += calc.c
SRC += accurate0.c
