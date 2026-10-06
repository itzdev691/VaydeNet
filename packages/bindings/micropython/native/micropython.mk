VAYDENET_MOD_DIR := $(USERMOD_DIR)
VAYDENET_REPO := $(abspath $(VAYDENET_MOD_DIR)/../../../..)
SRC_USERMOD_C += $(VAYDENET_MOD_DIR)/modvaydenet.c
SRC_USERMOD_LIB_CXX += $(VAYDENET_MOD_DIR)/bridge.cpp
SRC_USERMOD_LIB_CXX += $(VAYDENET_REPO)/packages/VaydeEngine/src/VaydeEngine.cpp
SRC_USERMOD_LIB_CXX += $(VAYDENET_REPO)/packages/VaydeEngine/src/PacketValidation.cpp
SRC_USERMOD_LIB_CXX += $(VAYDENET_REPO)/packages/VaydeEngine/src/PacketMessageDecoder.cpp
SRC_USERMOD_LIB_CXX += $(VAYDENET_REPO)/packages/VaydeEngine/src/PacketMessageEncoder.cpp
CFLAGS_USERMOD += -I$(VAYDENET_MOD_DIR)
CXXFLAGS_USERMOD += -I$(VAYDENET_MOD_DIR) -I$(VAYDENET_REPO)/packages/VaydeEngine/include -std=c++17
ifeq ($(shell uname -s),Darwin)
LIBS_USERMOD += -lc++
else
LIBS_USERMOD += -lstdc++
endif
