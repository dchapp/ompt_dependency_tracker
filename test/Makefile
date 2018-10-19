CXX = clang++ 
CFLAGS = -g -fopenmp -pedantic -std=c++14# -Wall -Werror -std=c++11
INCLUDE = -I/. -I/g/g17/chapp1/repos/LLVM-openmp/build/include -I/usr/tce/packages/boost/boost-1.66.0-mvapich2-2.2-intel-18.0.1/include

LDFLAGS = 
LDFLAGS += -L/usr/tce/packages/boost/boost-1.66.0-mvapich2-2.2-intel-18.0.1/lib
LD_LIBRARY_FLAGS = -L/. 
LD_LIBRARY_FLAGS += -L/g/g17/chapp1/repos/ompt_tools/dependency_tracker/build/lib -Wl,-rpath=/g/g17/chapp1/repos/ompt_tools/dependency_tracker/build/lib/libdependency_tracker.so 
LIBS= -ldependency_tracker  -lboost_system -lboost_thread

all: demo

.PHONY: demo
demo: demo.o 
	$(CXX) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LD_LIBRARY_FLAGS) $(LIBS)

%.o: %.cpp
	$(CXX) -c $(CPPFLAGS) $(CFLAGS) $(INCLUDE) $< -o $@


clean:
	rm -f *.o
	rm -f demo


