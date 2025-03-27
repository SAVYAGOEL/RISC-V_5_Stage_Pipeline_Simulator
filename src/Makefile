all: forward noforward

forward: forwarding.cpp
	g++ -o forward forwarding.cpp

noforward: main.cpp
	g++ -o noforward processor.cpp main.cpp

.PHONY: clean all
clean:
	rm -f forward noforward