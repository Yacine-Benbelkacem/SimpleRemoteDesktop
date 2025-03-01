
override CFLAGS += -DWAYLAND_ENV  -Wall -O3 -I./common -I./wayland_screenshot  -I/usr/include/pixman-1 -I/usr/include/libpng16
override TARGET ?= Linux

ifeq ($(TARGET), Linux)
	CC = cc
	LDFLAGS += -lc -lm  -ldl -lpthread -ljpeg -lwayland-client
	LDFLAGS_TGT += -lX11 -lXtst -lwayland-client `pkg-config --libs pixman-1`
endif

ifeq ($(TARGET), mingw64)
	CC = x86_64-w64-mingw32-gcc
	AR = x86_64-w64-mingw32-ar
	RESC = x86_64-w64-mingw32-windres
	LDFLAGS += -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic -ljpeg -lws2_32 -lmingw32
	LDFLAGS_TGT =
endif

sources_common := $(wildcard common/*.c)
objects_common := $(sources_common:common/%.c=obj/%.o)

sources_tgt := $(wildcard target/*.c)
objects_tgt := $(sources_tgt:target/%.c=obj/%.o)

sources_ctrl := $(wildcard control/*.c)
objects_ctrl := $(sources_ctrl:control/%.c=obj/%.o)

sources_wl := $(wildcard wayland_screenshot/*.c)
objects_wl := $(sources_wl:wayland_screenshot/%.c=obj/%.o)

all: output_dir SRDctrl SRDtgt

SRDctrl: $(objects_common) $(objects_ctrl)
	${CC} -o $@    $^ $(LDFLAGS) -lSDL2main -lSDL2

SRDtgt: $(objects_common) $(objects_tgt) $(objects_wl)
	${CC} -o $@    $^ $(LDFLAGS) $(LDFLAGS_TGT)

$(objects_tgt): obj/%.o: target/%.c
	${CC} -o $@ $^ -c $(CFLAGS) 

$(objects_common): obj/%.o: common/%.c
	${CC} -o $@ $^ -c $(CFLAGS) -lwayland-client

$(objects_ctrl): obj/%.o: control/%.c
	${CC} -o $@ $^ -c $(CFLAGS)

$(objects_wl): obj/%.o: wayland_screenshot/%.c
	${CC} -o $@ $^ -c $(CFLAGS) -lwayland-client `pkg-config --libs pixman-1`


output_dir:
	@mkdir -p obj

clean:
	rm -Rf  *.o  .*.o  .*.o.* SRDctrl SRDtgt obj *.exe
