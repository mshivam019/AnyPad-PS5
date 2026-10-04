include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
CFLAGS := -std=c11 -Wall -Wextra -Werror -O2 -Isrc -Iusb -DANYPAD_BIND_USER_PATH='"/data/redgear/bind_user"'
SRCS := usb/main.c src/ps5_vpad.c src/ps5_power.c src/lock.c src/log.c src/util.c
OBJS := $(patsubst %.c,build/redgear-usb/%.o,$(SRCS))
dist/Redgear-USB.elf: $(OBJS)
	@mkdir -p dist
	$(CC) -o $@ $^ -lScePad -lSceUserService -ldl
build/redgear-usb/%.o: %.c usb/report.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
