CPP11 = -std=c++11

dri = ../../../AllDriver/

vpath %.cpp ../src/
vpath %.cpp ../../src/
vpath %.out $(dri)

FILENAME = TCPClientBase SHMFIFO Common UDPBase MLGWConfig Driver main BaseTool
OBJ = $(addsuffix .o,$(FILENAME))
DEP = $(addsuffix .d,$(FILENAME))
OUT = KSPAVAMP.out

$(OUT): $(OBJ)
	g++ $^ $(CPP11) -o $(dri)$@ -ljsoncpp -lsqlite3 -lssl -lcrypto -ldl -lpthread

include $(DEP)

%.d: %.cpp
	@set -e
	@rm -f $@
	g++ -MM $< | sed 's/$*\.o[:]*/$*.o $@:/g' > $@

%.o: %.cpp
	g++ -c $(CPP11) $< -o $@

.PHONY:clean
clean:
	-rm -f $(OBJ) $(DEP) $(dri)$(OUT)