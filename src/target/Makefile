
override CFLAGS += -Wall -O3

sources := $(wildcard src/*.c)
objects := $(sources:src/%.c=obj/%.o)

all: output_dir SRDtgt

SRDtgt:  $(objects) message.o jpeg.o aes.o sha256.o utils.o cmd_param.o
    ${CC} -o $@    $^ $(LDFLAGS) -lpthread -lX11 -ljpeg

$(objects): obj/%.o: src/%.c
    ${CC} -o $@ $^ -c $(CFLAGS)

cmd_param.o: ../common/src/cmd_param.c
    ${CC} -o $@ $^ -c $(CFLAGS)

message.o: ../common/src/message.c
    ${CC} -o $@ $^ -c $(CFLAGS)

jpeg.o: ../common/src/jpeg.c
    ${CC} -o $@ $^ -c $(CFLAGS)

aes.o: ../common/src/aes.c
    ${CC} -o $@ $^ -c $(CFLAGS)

sha256.o: ../common/src/sha256.c
    ${CC} -o $@ $^ -c $(CFLAGS)

utils.o: ../common/src/utils.c
    ${CC} -o $@ $^ -c $(CFLAGS)

output_dir:
    @mkdir -p obj

clean:
    rm -Rf  *.o  .*.o  .*.o.* SRDtgt obj
