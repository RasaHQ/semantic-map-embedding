TARGET=smap
CXX=g++-10
CXXFLAGS=--std=c++17 -Wall -Wextra
BUILDDIR=build
SRCDIR=src
RUNSRCX=$(SRCDIR)/topo.cpp $(SRCDIR)/data.cpp $(SRCDIR)/argparse.cpp $(SRCDIR)/som.cpp $(SRCDIR)/smap.cpp $(SRCDIR)/utils.cpp
TESTSRCX=$(SRCDIR)/test/test_topo.cpp $(SRCDIR)/test/test_data.cpp $(SRCDIR)/test/test_som.cpp
RUNSRC=$(SRCDIR)/main.cpp $(RUNSRCX)
TESTSRC=$(SRCDIR)/test/test.cpp $(TESTSRCX) $(RUNSRCX)


all: release

release:
	$(CXX) $(CXXFLAGS) -O2 -s -static-libstdc++ -fopenmp $(RUNSRC) -o $(BUILDDIR)/$(TARGET)

sequential:
	$(CXX) $(CXXFLAGS) -O2 -s -static-libstdc++ $(RUNSRC) -o $(BUILDDIR)/$(TARGET)

debug:
	$(CXX) $(CXXFLAGS) -O0 -g $(RUNSRC) -o $(BUILDDIR)/$(TARGET)

tests:
	$(CXX) $(CXXFLAGS) -Og -g -fopenmp $(TESTSRC) -o $(BUILDDIR)/$(TARGET)-tests

clean:
	rm -f *.o
