BIN = a.out
OFILES = $(patsubst %.c,%.o,$(wildcard *.c))
CFLAGS += -Os -g $(shell pkg-config --cflags glib-2.0)
LDFLAGS += $(CFLAGS)
LDLIBS = -lgccjit $(shell pkg-config --libs glib-2.0)
ifeq ($(KEEPOBJ),)
	CLEANOBJ = $(wildcard *.o)
endif

ifneq ($(DEBUG),)
	CFLAGS += -DDEBUG
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
