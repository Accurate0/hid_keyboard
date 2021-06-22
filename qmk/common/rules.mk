UCIS_ENABLE = yes
WPM_ENABLE = yes

EXT_SRC += secrets.c

ifeq ($(strip $(SECRETS)), yes)
    OPT_DEFS += -DSECRETS
endif

EXT_SRC += libc.c
EXT_SRC += calc.c
EXT_SRC += accurate0.c
