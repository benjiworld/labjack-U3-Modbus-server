CC = gcc
CFLAGS = -O2 -Wall \
         -I. \
         -I/home/benjiworld/lj/exodriver-master/examples/U3 \
         -I/home/benjiworld/lj/exodriver-master/liblabjackusb

LDFLAGS = -L/home/benjiworld/lj/exodriver-master/liblabjackusb \
          -llabjackusb -lusb-1.0 -lm

OBJS = main.o modbus_u3.o u3.o u3_easy_wrap.o

TARGET = modbus_u3_server
BINDIR = /usr/local/bin
SYSTEMD_DIR = /etc/systemd/system
SERVICE_NAME = modbus-u3

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

u3.o: /home/benjiworld/lj/exodriver-master/examples/U3/u3.c
	$(CC) $(CFLAGS) -c $< -o $@

u3_easy_wrap.o: u3_easy_wrap.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -d $(BINDIR)
	install -m 0755 $(TARGET) $(BINDIR)/$(TARGET)

systemd: install
	@echo "Creating systemd service file $(SERVICE_NAME).service..."
	@printf "[Unit]\n\
Description=Modbus TCP server for LabJack U3\n\
After=network.target\n\
\n\
[Service]\n\
Type=simple\n\
ExecStart=$(BINDIR)/$(TARGET)\n\
Restart=always\n\
User=root\n\
\n\
[Install]\n\
WantedBy=multi-user.target\n" > $(SERVICE_NAME).service
	sudo install -m 0644 $(SERVICE_NAME).service $(SYSTEMD_DIR)/$(SERVICE_NAME).service
	sudo systemctl daemon-reload
	@echo "Service installed. Enable and start with:"
	@echo "  sudo systemctl enable $(SERVICE_NAME)"
	@echo "  sudo systemctl start $(SERVICE_NAME)"

uninstall:
	rm -f $(BINDIR)/$(TARGET)
	rm -f $(SYSTEMD_DIR)/$(SERVICE_NAME).service
	sudo systemctl daemon-reload
	@echo "Binary and systemd service removed."

.PHONY: all clean install systemd uninstall
