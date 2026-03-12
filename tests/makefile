CC ?= gcc
CFLAGS ?= -g

BIN = test_minimal_regression

SRCS = test_minimal_regression.c \
	../siemens_plc_s7_net/dynstr.c \
	../siemens_plc_s7_net/siemens_helper.c \
	../siemens_plc_s7_net/siemens_s7.c \
	../siemens_plc_s7_net/siemens_s7_comm.c \
	../siemens_plc_s7_net/socket.c \
	../siemens_plc_s7_net/utill.c

OBJS = $(SRCS:.c=.o)

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -I../siemens_plc_s7_net -c $< -o $@

clean:
	rm -f $(OBJS) $(BIN)
