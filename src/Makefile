NAME		=	webserv

CXX			=	c++
CXXFLAGS	=	-Wall -Wextra -Werror -std=c++98 -MMD
CPPFLAGS	=	
DEBUGFLAGS	=	-g -fsanitize=address -fsanitize=undefined
RM			=	rm -f

ifeq ($(shell uname), Linux)
SRC			=	io/Epoll.cpp
else
SRC			=	io/Kqueue.cpp
endif

SRC			+=	main.cpp\
				Logger.cpp\
				ServerManager.cpp\
				VirtualServer.cpp\
				Location.cpp\
\
				event/EventObject.cpp\
				event/Client.cpp\
				event/Server.cpp\
				event/Cgi.cpp\
\
				parser/ConfigParser.cpp\
				parser/LocationParser.cpp\
				parser/ServerParser.cpp\
				parser/HttpRequestParser.cpp\
\
				tokenizer/HttpStreamTokenizer.cpp\
				tokenizer/FileTokenizer.cpp\
\
				exception/ConfigParserException.cpp\
				exception/HttpErrorHandler.cpp\
\
				http/FindLocation.cpp\
				http/AutoIndex.cpp\
				http/RequestHandler.cpp\
\
				responder/AResponder.cpp\
				responder/GetResponder.cpp\
				responder/HeadResponder.cpp\
				responder/PutResponder.cpp\
				responder/PostResponder.cpp\
				responder/DeleteResponder.cpp\
\
				io/Buffer.cpp\
\
				util/Util.cpp\
\
				socket/Tcp.cpp\


OBJ			:=	$(SRC:%.cpp=%.o)
DEP			:=	$(OBJ:%.o=%.d)

INCL_PATH	=	-I./\
				-I../include\


STATUS		=	$(shell ls DEBUG.mode 2>/dev/null)
ifeq ($(STATUS), DEBUG.mode)
CXXFLAGS	+=	$(DEBUGFLAGS)
COMPILE_MODE=	DEBUG.mode
else
COMPILE_MODE=	RELEASE.mode
endif


ifndef WEBSERV_ROOT
WEBSERV_ROOT=	$(shell pwd)
WEBSERV_ROOT:=	$(WEBSERV_ROOT:%/src=%)
endif
CPPFLAGS	+=	-DWEBSERV_ROOT='"$(WEBSERV_ROOT)/"'

ifndef TEMP_DIR
TEMP_DIR	=	$(shell echo $(HOME)/goinfre)
endif
CPPFLAGS	+=	-DTEMP_DIR='"$(TEMP_DIR)/"'


.PHONY: all debug release clean fclean re

all: $(COMPILE_MODE)
	$(MAKE) $(NAME) -j4

release: RELEASE.mode
	$(MAKE) all

debug: DEBUG.mode
	$(MAKE) all

RELEASE.mode:
	$(MAKE) fclean
	touch RELEASE.mode

DEBUG.mode:
	$(MAKE) fclean
	touch DEBUG.mode

clean:
	$(RM) $(OBJ)
	$(RM) $(DEP)
	$(RM) RELEASE.mode DEBUG.mode

fclean: clean
	$(RM) $(NAME)

re: fclean
	$(MAKE) all

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^ $(INCL_PATH)

$(OBJ): %.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $< $(INCL_PATH)

-include $(DEP)
