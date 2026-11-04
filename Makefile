.PHONY: all examples test clean

CXX = g++
CXXFLAGS = -std=c++20 -I./include

LIB_SRCS = src/vulcan.cpp src/feature_registry.cpp src/feature_store.cpp
LIB_OBJS = $(LIB_SRCS:.cpp=.o)

EXAMPLES = build/examples/ssd \
           build/examples/cwnd build/examples/cbl \
           build/examples/evict_batch build/examples/region_weights

all: examples benchmark_rank

examples: $(EXAMPLES)

build/examples/%: examples/%.cpp $(LIB_OBJS)
	mkdir -p build/examples
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIB_OBJS)

benchmark_rank: benchmark/rank.cpp $(LIB_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ benchmark/rank.cpp $(LIB_OBJS)

# Tests
test_store: test/store/test_store.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -I./test -o test/store/test_store.o test/store/test_store.cpp src/vulcan.o src/feature_registry.o src/feature_store.o


test_min_max: test/listeners/test_min_max.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -I./test -o test/listeners/test_min_max.o test/listeners/test_min_max.cpp src/vulcan.o src/feature_registry.o src/feature_store.o


test_rolling_window: test/listeners/test_rolling_window.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -I./test -o test/listeners/test_rolling_window.o test/listeners/test_rolling_window.cpp src/vulcan.o src/feature_registry.o src/feature_store.o


test_population_percentile: test/listeners/test_population_percentile.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -I./test -o test/listeners/test_population_percentile.o test/listeners/test_population_percentile.cpp src/vulcan.o src/feature_registry.o src/feature_store.o


test_rolling_count: test/listeners/test_rolling_count.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -I./test -o test/listeners/test_rolling_count.o test/listeners/test_rolling_count.cpp src/vulcan.o src/feature_registry.o src/feature_store.o

test_ewma: test/listeners/test_ewma.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -I./test -o test/listeners/test_ewma.o test/listeners/test_ewma.cpp src/vulcan.o src/feature_registry.o src/feature_store.o

test_average: test/listeners/test_average.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -I./test -o test/listeners/test_average.o test/listeners/test_average.cpp src/vulcan.o src/feature_registry.o src/feature_store.o

test_rolling_percentile: test/listeners/test_rolling_percentile.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -I./test -o test/listeners/test_rolling_percentile.o test/listeners/test_rolling_percentile.cpp src/vulcan.o src/feature_registry.o src/feature_store.o

test_registry: test/registry/test_registry.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -I./test -o test/registry/test_registry.o test/registry/test_registry.cpp src/vulcan.o src/feature_registry.o src/feature_store.o

test_initial_values: test/store/test_initial_values.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -I./test -o test/store/test_initial_values.o test/store/test_initial_values.cpp src/vulcan.o src/feature_registry.o src/feature_store.o

test_groups: test/store/test_groups.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -I./test -o test/store/test_groups.o test/store/test_groups.cpp src/vulcan.o src/feature_registry.o src/feature_store.o

test_configs: test/listeners/test_configs.cpp src/vulcan.o src/feature_registry.o src/feature_store.o
	$(CXX) $(CXXFLAGS) -I./test -o test/listeners/test_configs.o test/listeners/test_configs.cpp src/vulcan.o src/feature_registry.o src/feature_store.o

test: test_rolling_count test_min_max test_population_percentile test_rolling_window test_store test_ewma test_configs test_groups test_initial_values test_average test_rolling_percentile test_registry
	./test/listeners/test_rolling_count.o
	./test/listeners/test_min_max.o
	./test/listeners/test_population_percentile.o
	./test/listeners/test_rolling_window.o
	./test/store/test_store.o
	./test/listeners/test_ewma.o
	./test/listeners/test_configs.o
	./test/store/test_groups.o
	./test/store/test_initial_values.o
	./test/listeners/test_average.o
	./test/listeners/test_rolling_percentile.o
	./test/registry/test_registry.o

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	find . -name "*.o" -delete
	rm -rfv benchmark_rank $(EXAMPLES)