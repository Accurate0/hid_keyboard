UCIS_ENABLE = yes
WPM_ENABLE = yes

ifndef NO_SECRETS
SRC += secrets.c
endif

SRC += calc.c
SRC += accurate0.c
