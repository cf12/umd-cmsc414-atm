CC = gcc
DEPS = rsa/rsa.c

ifeq ($(CC),clang)
	STACK_FLAGS = -fno-stack-protector -Wl,-allow_stack_execute
else
	STACK_FLAGS = -fno-stack-protector -z execstack -g
endif

CFLAGS = ${STACK_FLAGS} \
	-I/opt/homebrew/opt/openssl/include \
	-I/usr/include/openssl \
	-lcrypto -lssl -Wall -Iutil -Iatm -Ibank -Irouter -Irsa -I \
	-ftrapv.

all: bin bin/atm bin/bank bin/router

bin:
	mkdir -p bin

bin/atm : atm/atm-main.c atm/atm.c
	${CC} ${DEPS} atm/atm.c atm/atm-main.c -o bin/atm ${CFLAGS}

bin/bank : bank/bank-main.c bank/bank.c util/hash_table.c util/list.c
	${CC} ${DEPS} bank/bank.c bank/bank-main.c util/hash_table.c util/list.c -o bin/bank ${CFLAGS} -lm

bin/router : router/router-main.c router/router.c
	${CC} ${DEPS} router/router.c router/router-main.c -o bin/router ${CFLAGS}

test : util/list.c util/list_example.c util/hash_table.c util/hash_table_example.c
	${CC} ${DEPS} util/list.c util/list_example.c -o bin/list-test ${CFLAGS}
	${CC} ${DEPS} util/list.c util/hash_table.c util/hash_table_example.c -o bin/hash-table-test ${CFLAGS}

clean:
	cd bin && rm -f *

clean-card:
	find . -name \*.card -type f -delete