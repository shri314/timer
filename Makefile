all:
	g++ -fsanitize=address -std=c++17 timer.cpp -pthread && ./a.out 2>&1
	g++ -fsanitize=thread -std=c++17 timer.cpp -pthread && ./a.out 2>&1
	g++ -fsanitize=undefined -std=c++17 timer.cpp -pthread && ./a.out 2>&1

clean:
	rm -f a.out
