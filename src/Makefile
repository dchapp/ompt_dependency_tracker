CXX=clang++
CFLAGS= -g -pedantic -std=c++14 -fopenmp -fPIC  #-Wall # -Werror 
INCLUDE= -I/. -I/ompt -I$(HOME)/repos/LLVM-openmp/build/include -I/usr/tce/packages/boost/boost-1.66.0-mvapich2-2.2-intel-18.0.1/include

LDFLAGS= -L/. 
# This is necessary to link w/ correct boost (1.66.0) rather than default system
# boost (which is currently 1.53.0 but may change) 
LDFLAGS+= -L/usr/tce/packages/boost/boost-1.66.0-mvapich2-2.2-intel-18.0.1/lib

LD_LIBRARY_FLAGS= -L/. 
LD_LIBRARY_FLAGS+= -Wl,-rpath=/usr/tce/packages/intel/intel-16.0.3/tbb/lib/intel64/gcc4.4

LIBS= -lboost_system -lboost_thread -ltbb -lbfd

all: dependency_tracker 
	

dependency_tracker: 
	$(CXX) $(CFLAGS) $(INCLUDE) -c dependency_tracker.cpp -o dependency_tracker.o
	$(CXX) -shared -o libdependency_tracker.so dependency_tracker.o $(LDFLAGS) $(LD_LIBRARY_FLAGS) $(LIBS)
	cp libdependency_tracker.so ../build/lib/libdependency_tracker.so 


clean:
	rm -f *.o
	rm -f *.so 
	rm -f *.a 


