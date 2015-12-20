# oscpack makefile

# the source code should auto-detect endianess for most systems
# (see osc/OscHostEndianness.h)
# otherwise you need to explicitly set ENDIANESS below
# to either OSC_HOST_BIG_ENDIAN or OSC_HOST_LITTLE_ENDIAN
# Apple Mac OS X (PowerPC): OSC_HOST_BIG_ENDIAN
# Apple Mac OS X (Intel): OSC_HOST_LITTLE_ENDIAN
# Win32: OSC_HOST_LITTLE_ENDIAN
# i386 GNU/Linux: OSC_HOST_LITTLE_ENDIAN

ENDIANESS=OSC_DETECT_ENDIANESS #source code will detect using preprocessor
#ENDIANESS=OSC_HOST_LITTLE_ENDIAN

UNAME := $(shell uname)

CXX := g++ 
INCLUDES := -I. -I./lib/
COPTS  := -Wall -Wextra -Ofast -funroll-loops -std=c++11
CDEBUG := -Wall -Wextra -g 
CXXFLAGS := $(COPTS) $(INCLUDES) -D$(ENDIANESS)

BINDIR := bin
PREFIX := /usr/local
INSTALL := install -c

#Name definitions
PASSTHROUGH := $(BINDIR)/visionp

INCLUDEDIR := oscpack
LIBNAME := liboscpack
LIBSONAME := $(LIBNAME).so
LIBFILENAME := $(LIBSONAME).1.1.0

# Common source groups

RECEIVESOURCES := lib/osc/OscReceivedElements.cpp lib/osc/OscPrintReceivedElements.cpp
SENDSOURCES := lib/osc/OscOutboundPacketStream.cpp
NETSOURCES := lib/ip/posix/UdpSocket.cpp lib/ip/IpEndpointName.cpp lib/ip/posix/NetworkingUtils.cpp
COMMONSOURCES := lib/osc/OscTypes.cpp

RECEIVEOBJECTS := $(RECEIVESOURCES:.cpp=.o)
SENDOBJECTS := $(SENDSOURCES:.cpp=.o)
NETOBJECTS := $(NETSOURCES:.cpp=.o)
COMMONOBJECTS := $(COMMONSOURCES:.cpp=.o)

# Source

PASSSOURCES := src/visionp.cpp
PASSOBJECTS := $(PASSSOURCES:.cpp=.o)

#Library objects

LIBOBJECTS := $(COMMONOBJECTS) $(SENDOBJECTS) $(RECEIVEOBJECTS) $(NETOBJECTS)

.PHONY: all pass library install install-local

all: pass
clean:
	rm -f src/*.o lib/osc/*.o lib/ip/*.o lib/ip/posix/*.o lib/ip/win32/*.o

pass : $(PASSTHROUGH)


# Build rule and common dependencies for all programs
# | specifies an order-only dependency so changes to bin dir modified date don't trigger recompile
$(PASSTHROUGH) : $(COMMONOBJECTS) | $(BINDIR)
	$(CXX) -o $@ $^ -lpthread

# Additional dependencies for each program (make accumulates dependencies from multiple declarations)
$(PASSTHROUGH) : $(PASSOBJECTS) $(SENDOBJECTS) $(NETOBJECTS)

$(BINDIR):
	mkdir $@

$(LIBFILENAME): $(LIBOBJECTS)
ifeq ($(UNAME), Darwin)
	#Mac OS X case
	$(CXX) -dynamiclib -Wl,-install_name,$(LIBSONAME) -o $(LIBFILENAME) $(LIBOBJECTS) -lc
else
	#GNU/Linux case
	$(CXX) -shared -Wl,-soname,$(LIBSONAME) -o $(LIBFILENAME) $(LIBOBJECTS)
endif

lib: $(LIBFILENAME)

#Installs the library on a system global location
install: $(LIBFILENAME)
	@$(INSTALL) -m 755 $(LIBFILENAME) $(PREFIX)/lib/$(LIBFILENAME)
	@ln -s -f $(PREFIX)/lib/$(LIBFILENAME) $(PREFIX)/lib/$(LIBSONAME) 
	@mkdir  -p $(PREFIX)/include/oscpack/ip $(PREFIX)/include/oscpack/osc
	@$(INSTALL) -m 644 lib/ip/*.h $(PREFIX)/include/oscpack/ip
	@$(INSTALL) -m 644 lib/osc/*.h $(PREFIX)/include/oscpack/osc
	@echo "SUCCESS! oscpack has been installed in $(PREFIX)/lib and $(PREFIX)/include/ospack/"
ifneq ($(UNAME), Darwin)
	@echo "now doing ldconfig..."
	@ldconfig
endif

#Installs the include/lib structure locally
install-local: $(LIBFILENAME)
	@echo ""
	@echo " Installing in local directory <$(INCLUDEDIR)>"
	@echo "   > Creating symbolic link"
	@ln -s $(LIBFILENAME) $(LIBSONAME)
	@echo "   > Creating directories"
	@mkdir -p oscpack/lib
	@mkdir -p oscpack/include/ip
	@mkdir -p oscpack/include/osc
	@echo "   > Copying files"
	@mv $(LIBFILENAME) $(LIBSONAME) oscpack/lib
	@cp lib/ip/*.h oscpack/include/ip
	@cp lib/osc/*.h oscpack/include/osc
	@echo ""
	@echo "   > Success!"

