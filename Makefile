NAME		= ft_ping

CC			= gcc
CFLAGS		= -Wall -Wextra -Werror
LDFLAGS		= -lm

COLOR_RESET	= \033[0m
COLOR_BOLD	= \033[1m
COLOR_RED	= \033[31m
COLOR_GREEN	= \033[32m
COLOR_YELLOW	= \033[33m
COLOR_BLUE	= \033[34m

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
	@printf "$(COLOR_BOLD)$(COLOR_BLUE)LD$(COLOR_RESET)  %s\n" "$(NAME)"
	@$(CC) $(CFLAGS) $(OBJS) -o $(NAME) $(LDFLAGS)
	@printf "$(COLOR_BOLD)$(COLOR_GREEN)OK$(COLOR_RESET)  %s\n" "$(NAME)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@printf "$(COLOR_BOLD)$(COLOR_YELLOW)CC$(COLOR_RESET)  %s\n" "$<"
	@$(CC) $(CFLAGS) -MMD -MP -I$(INC_DIR) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

-include $(DEPS)

clean:
	@printf "$(COLOR_BOLD)$(COLOR_RED)CLEAN$(COLOR_RESET) %s\n" "$(OBJ_DIR)"
	@rm -rf $(OBJ_DIR)

fclean: clean
	@printf "$(COLOR_BOLD)$(COLOR_RED)FCLEAN$(COLOR_RESET) %s\n" "$(NAME)"
	@rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
