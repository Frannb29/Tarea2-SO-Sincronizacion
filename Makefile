CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
TSANFLAGS = -std=c++17 -O1 -g -Wall -Wextra -pthread -fsanitize=thread

SRCS = rw.cpp monitor_rw.cpp control_atomico_rw.cpp verificador.cpp
OBJS = $(SRCS:.cpp=.o)
HEADERS = common.h monitor_rw.h control_atomico_rw.h
TARGET = rw

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Regla extra para buscar carreras de datos con ThreadSanitizer
tsan: $(SRCS) $(HEADERS)
	$(CXX) $(TSANFLAGS) $(SRCS) -o $(TARGET)_tsan

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET)_tsan

.PHONY: all tsan clean