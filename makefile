# Makefile

# 编译器
CXX = g++

# 编译选项
CXXFLAGS = -Wall -Wextra -g

# 目标目录
BUILD_DIR = build
SRC_DIR = src
LIB_DIR = lib

# 源文件
CLIENT_SRC = $(SRC_DIR)/client/client-main.cpp $(LIB_DIR)/m-net.cpp $(LIB_DIR)/file-packet.cpp
SERVER_SRC = $(SRC_DIR)/server/server-main.cpp $(LIB_DIR)/m-net.cpp $(LIB_DIR)/file-packet.cpp

# 目标文件
CLIENT_BIN = $(BUILD_DIR)/client
SERVER_BIN = $(BUILD_DIR)/server

# 默认目标
all: $(CLIENT_BIN) $(SERVER_BIN)

# 客户端目标
$(CLIENT_BIN): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

# 服务器目标
$(SERVER_BIN): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

# 清理目标
clean:
	rm -f $(BUILD_DIR)/client $(BUILD_DIR)/server
