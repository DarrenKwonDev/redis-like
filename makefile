
CXX = clang++
CXX_STANDARD = c++20
CXX_WARNINGS = -Wall -Wfatal-errors -Wshadow -Wnon-virtual-dtor -Wcast-align

CXX_STD_WARNINGS = -std=$(CXX_STANDARD) $(CXX_WARNINGS)

CXX_COMPILER_CALL = $(CXX) $(CXX_STD_WARNINGS)

build: client_debug server_debug

client_debug: 
	$(CXX_COMPILER_CALL) ./client.cpp ./utils.cpp -o client -g

server_debug: 
	$(CXX_COMPILER_CALL) ./server.cpp ./utils.cpp -o server -g

compile_commands.json:
	bear -- make build

clean:
	rm -rf client server

.PHONY: build clean client_debug server_debug