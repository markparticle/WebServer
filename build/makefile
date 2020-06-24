#heaptimer.cpp 
CXX = g++
CFLAGS = -std=c++14 -g 

TARGET = server
OBJS = main.cpp log.cpp sqlconnpool.cpp httpconn.cpp heaptimer.cpp webserver.cpp 

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(TARGET)  -pthread -lmysqlclient

clean:
	rm -rf $(OBJS) $(TARGET)