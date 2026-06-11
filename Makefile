.PHONY: test

CXX = g++
CXXFLAGS = -std=c++20 -I./include

LIB_SRCS = src/vulcan.cpp src/feature_registry.cpp src/feature_store.cpp
LIB_OBJS = $(LIB_SRCS:.cpp=.o)

all: value_example rank_example value_rank_example benchmark_rank

value_example: value_example.cpp $(LIB_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ value_example.cpp $(LIB_OBJS)

rank_example: rank_example.cpp $(LIB_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ rank_example.cpp $(LIB_OBJS)

value_rank_example: value_rank_example.cpp $(LIB_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ value_rank_example.cpp $(LIB_OBJS)

benchmark_rank: benchmark/rank.cpp $(LIB_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ benchmark/rank.cpp $(LIB_OBJS)

# Tests
test_safety: test/test_safety.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -o test/test_safety.o test/test_safety.cpp src/vulcan.o src/feature_registry.o src/feature_store.o


test_minmaxavg: test/test_minmaxavg.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -o test/test_minmaxavg.o test/test_minmaxavg.cpp src/vulcan.o src/feature_registry.o src/feature_store.o


test_rolling_window: test/test_rolling_window.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -o test/test_rolling_window.o test/test_rolling_window.cpp src/vulcan.o src/feature_registry.o src/feature_store.o


test_percentile: test/test_percentile.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -o test/test_percentile.o test/test_percentile.cpp src/vulcan.o src/feature_registry.o src/feature_store.o


test_count: test/test_count.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -o test/test_count.o test/test_count.cpp src/vulcan.o src/feature_registry.o src/feature_store.o

test_ewma: test/test_ewma.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -o test/test_ewma.o test/test_ewma.cpp src/vulcan.o src/feature_registry.o src/feature_store.o

test: test_count test_minmaxavg test_percentile test_rolling_window test_safety test_ewma
	./test/test_count.o
	./test/test_minmaxavg.o
	./test/test_percentile.o
	./test/test_rolling_window.o
	./test/test_safety.o
	./test/test_ewma.o

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	find . -name "*.o" -delete
	rm -rfv value_example rank_example value_rank_example benchmark_rank