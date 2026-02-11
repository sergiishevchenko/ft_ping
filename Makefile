NAME		= ft_ping

CC			= gcc
CFLAGS		= -Wall -Wextra -Werror
LDFLAGS		= -lm

SRC_DIR		= srcs
INC_DIR		= includes
OBJ_DIR		= obj

SRCS		= $(SRC_DIR)/main.c \
			  $(SRC_DIR)/socket.c \
			  $(SRC_DIR)/dns.c \
			  $(SRC_DIR)/send.c \
			  $(SRC_DIR)/recv.c \
			  $(SRC_DIR)/checksum.c \
			  $(SRC_DIR)/stats.c \
			  $(SRC_DIR)/print.c \
			  $(SRC_DIR)/signal.c \
			  $(SRC_DIR)/utils.c

OBJS		= $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS		= $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -I$(INC_DIR) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

-include $(DEPS)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
