PROGS = big_aoa_counter
SRCS = big_aoa_counter.c
OBJS = $(SRCS:.c=.o)
LIBDIR = lib

PKGS = glib-2.0 gio-2.0 cairo jansson axparameter libcurl

CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))
LDLIBS += -laxoverlay -lcivetweb -lpthread -lm

CFLAGS += -I/opt/build/civetweb/include
LDFLAGS += -L$(LIBDIR) -Wl,--no-as-needed,-rpath,'$$ORIGIN/lib'

CFLAGS += -Wall \
          -Wextra \
          -Wformat=2 \
          -Wpointer-arith \
          -Wbad-function-cast \
          -Wstrict-prototypes \
          -Wmissing-prototypes \
          -Winline \
          -Wdisabled-optimization \
          -Wfloat-equal \
          -W \
          -Werror

all: $(PROGS)

$(PROGS): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) $(LDLIBS) -o $@

clean:
	rm -rf $(PROGS) *.o *.eap* *_LICENSE.txt package.conf* param.conf tmp* $(LIBDIR)
