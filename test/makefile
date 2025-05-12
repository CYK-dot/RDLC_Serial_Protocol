CXX := g++
CXXFLAGS := -std=c++17 -Wall -Ilib/headers/ -I.. -I.
LDFLAGS := -pthread

LIB_DIR := lib
LIBS := $(LIB_DIR)/libgtest.a $(LIB_DIR)/libgmock.a $(LIB_DIR)/libgtest_main.a $(LIB_DIR)/libgmock_main.a

TARGET := test
SRC := ../rdlc.c rdlcTest.cpp rdlcCriticalTest.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) $(LDFLAGS) -o $@

clean:
	rm -f $(TARGET)
