

client_debug: 
	clang++ -std=c++20 ./client.cpp ./utils.cpp -o client -g

server_debug: 
	clang++ -std=c++20 ./server.cpp ./utils.cpp -o server -g