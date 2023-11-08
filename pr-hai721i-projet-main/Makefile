########################################
#~ Définitions
########################################


CC=gcc

# Pour contrôler le niveau de logs souhaité
#	0 : Aucun log
#	1 : Logs d'erreurs uniquement
#	2 : Logs d'erreur et infos 
#	3 : Tous (debug)
LOG_LEVEL=2


# Binaries
BIN=bin/server bin/node bin/check-coloration

# Sources
SRC_DIR=src
SRC_SERVER=server.c
SRC_NODE=node.c


default: $(BIN)

########################################
#~ Règles
########################################

obj/%.o: $(SRC_DIR)/%.c
		@ mkdir -p obj
		$(CC) -DLOG_LEVEL=$(LOG_LEVEL) -Wall -Iinclude -c $< -o $@ -pthread

bin/server: obj/server.o obj/message.o obj/tcp.o
		@ mkdir -p bin
		$(CC) -o $@ $+ -pthread

bin/node: obj/node.o obj/random_color.o obj/message.o obj/tcp.o
		@ mkdir -p bin
		$(CC) -o $@ $+ -pthread

bin/check-coloration : obj/check_coloration.o
		@mkdir -p bin
		$(CC) -o $@ $+

clean:
		rm -f $(BIN) obj/*.o *~
