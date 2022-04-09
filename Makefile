BIN = a.out
OFILES = $(patsubst %.c,%.o,$(wildcard *.c))
CFLAGS += -O3 -g
LDFLAGS += $(CFLAGS)
LDLIBS =
ifeq ($(KEEPOBJ),)
	CLEANOBJ = $(wildcard *.o)
endif
CLEAN = $(wildcard $(CLEANOBJ) $(BIN))

all: $(BIN)

$(BIN): $(OFILES)
	$(info linking: $(OFILES))
	$(CC) -o $@ $(OFILES) $(LDFLAGS) $(LDLIBS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
ifneq ($(CLEAN),)
	$(RM) $(CLEAN)
endif
