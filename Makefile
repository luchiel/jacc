.PHONY: all clean test

APPNAME = jacc
SOURCES_PATH = src
OBJECTS_PATH = obj
SOURCES = $(wildcard $(SOURCES_PATH)/*.c)
OBJECTS = $(patsubst $(SOURCES_PATH)/%.o, $(OBJECTS_PATH)/%.o, $(patsubst %.c, %.o, $(SOURCES)))
CFLAGS += -g -Wall -Wextra -Wno-switch

test: all
	python test.py

all: $(APPNAME)

$(OBJECTS): | $(OBJECTS_PATH)

$(OBJECTS_PATH):
	mkdir $@

clean:
	$(RM) $(APPNAME) $(OBJECTS)

$(OBJECTS_PATH)/%.o: $(SOURCES_PATH)/%.c
	$(CC) -c -o $@ $< $(CFLAGS) $(LDFLAGS)

$(APPNAME): $(OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)