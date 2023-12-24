

client_debug: 
	clang++ ./client.cpp ./utils.cpp -o client -g

server_debug: 
	clang++ ./server.cpp ./utils.cpp -o server -g