CC = gcc
#EXODRIVER ?= /home/benjiworld/lj/exodriver-master
CFLAGS = -O2 -Wall -Wextra
CPPFLAGS += -I. -I$(EXODRIVER)/examples/U3 -I$(EXODRIVER)/liblabjackusb
LDFLAGS += -L$(EXODRIVER)/liblabjackusb
LDLIBS = -llabjackusb -lusb-1.0 -lm
BINDIR ?= /usr/local/bin
all: modbus_u3_server modbus_inspect_all
modbus_u3_server: main.o modbus_u3.o u3.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
main.o modbus_u3.o: modbus_u3.h
u3.o: $(EXODRIVER)/examples/U3/u3.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-unused-parameter -c $< -o $@
modbus_inspect_all: modbus_inspect_all.c
	$(CC) $(CFLAGS) -o $@ $<
install: modbus_u3_server
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $< $(DESTDIR)$(BINDIR)/$<
systemd: install
	install -m 0644 modbus-u3.service /etc/systemd/system/modbus-u3.service
	systemctl daemon-reload
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/modbus_u3_server
	rm -f /etc/systemd/system/modbus-u3.service
	systemctl daemon-reload
clean:
	rm -f main.o modbus_u3.o u3.o modbus_u3_server modbus_inspect_all
.PHONY: all install systemd uninstall clean
