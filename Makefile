CC = gcc
DEPS = util/hash_table.c util/list.c util/rsa.c util/packet.c

ifeq ($(CC),clang)
	STACK_FLAGS = -fno-stack-protector -Wl,-allow_stack_execute
else
	STACK_FLAGS = -fno-stack-protector -g -O0
endif

CFLAGS = ${STACK_FLAGS} \
	-I/opt/homebrew/opt/openssl/include \
	-I/usr/include/openssl \
	-lcrypto -lssl -Wall -Iutil -Iatm -Ibank -Irouter -Irsa -I \
	-ftrapv.

all: bin bin/init bin/atm bin/bank bin/router

bin:
	mkdir -p bin

bin/init:
	cp init bin/

bin/atm : atm/atm-main.c atm/atm.c ${DEPS}
	${CC} atm/atm.c atm/atm-main.c ${DEPS} -o bin/atm ${CFLAGS}

bin/bank : bank/bank-main.c bank/bank.c ${DEPS}
	${CC} bank/bank.c bank/bank-main.c ${DEPS} -o bin/bank ${CFLAGS} -lm

bin/router : router/router-main.c router/router.c
	${CC} router/router.c router/router-main.c -o bin/router ${CFLAGS}

test : util/list.c util/list_example.c util/hash_table.c util/hash_table_example.c
	${CC} util/list.c util/list_example.c -o bin/list-test ${CFLAGS}
	${CC} util/list.c util/hash_table.c util/hash_table_example.c -o bin/hash-table-test ${CFLAGS}

clean:
	cd bin && rm -f *

clean-card:
	find . -name \*.card -type f -delete