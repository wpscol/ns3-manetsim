-include .env.example
-include .env

TIMEDATE_STR := $(shell date +"%H-%M_%d-%m-%Y")

VENV_PATH := ./.venv
PYTHON_BIN := $(VENV_PATH)/bin/python3

RAND_VAL := $(shell echo $$RANDOM)

default: init

init: cpenv download rmdefault link configure venv

run: run_ns3 analyze

build:
	$(NS3_BIN) build $(NS3_ADHOC_SIM_SRC)

download:
	wget 'https://www.nsnam.org/releases/ns-allinone-3.44.tar.bz2'
	tar xvf ns-allinone-3.44.tar.bz2
	rm ns-allinone-3.44.tar.bz2

rmdefault:
	rm -rf $(NS3_DIR)/scratch

venv:
	/usr/bin/python3 -m venv .venv
	$(PYTHON_BIN) -m pip install \
		pandas \
		matplotlib \
		black

clean:
	rm -rf $(NS3_AIO_DIR)
	rm -rf ./.venv
	rm -rf ./output

configure:
	$(NS3_BIN) configure
	$(NS3_BIN) build

cpenv:
	cp -f --update=none .env.example .env

link:
	ln -sfn $(shell pwd)/scratch $(NS3_DIR)/scratch

analyze:
	echo $(SIM_RNG_RUNS) | tr ' ' '\n' | /usr/bin/parallel \
		$(PYTHON_BIN) ./scripts/analyze_results.py \
			--nodes=$(SIM_NODES_NUM) \
			--packets="$(SIM_RESULTS_PATH)/$(TIMEDATE_STR)/{}/packets.csv" \
			--movement="$(SIM_RESULTS_PATH)/$(TIMEDATE_STR)/{}/movement.csv" \
			--connectivity="$(SIM_RESULTS_PATH)/$(TIMEDATE_STR)/{}/connectivity.csv" \
			--plot="$(SIM_RESULTS_PATH)/$(TIMEDATE_STR)/{}/movement_plot.png" \
			--xmax="$(SIM_AREA_SIZE_X)" \
			--ymax="$(SIM_AREA_SIZE_Y)"

run_ns3:
	echo $(SIM_RNG_RUNS) | tr ' ' '\n' | /usr/bin/parallel \
		$(NS3_DIR)/$(NS3_ADHOC_SIM_BIN) \
			--rngRun={} \
			--rngSeed=$(SIM_RNG_SEED) \
			--simulationTime=$(SIM_TIME) \
			--warmupTime=$(SIM_WARMUP_TIME) \
			--samplingFreq=$(SIM_SAMPLING_FREQ) \
			--nodesNum=$(SIM_NODES_NUM) \
			--spineNodesPercent=$(SIM_SPINE_NODES_PERCENT) \
			--spineVariant=$(SIM_SPINE_VARIANT) \
			--areaSizeX=$(SIM_AREA_SIZE_X) \
			--areaSizeY=$(SIM_AREA_SIZE_Y) \
			--packetsPerSecond=$(SIM_PACKETS_PER_SECOND) \
			--packetsSize=$(SIM_PACKET_SIZE) \
			--wifiChannelWidth=$(SIM_WIFI_CHANNEL_WIDTH) \
			--environment=$(SIM_ENV_TARGET) \
			--treeCount=$(SIM_ENV_FOREST_TREE_COUNT) \
			--treeSize=$(SIM_ENV_FOREST_TREE_SIZE) \
			--treeHeight=$(SIM_ENV_FOREST_TREE_HEIGHT) \
			--scenario=$(SIM_SCENARIO) \
			--wipeDirection=$(SIM_SCENARIO_WIPE_DIRECTION) \
			--wipeSpeed=$(SIM_SCENARIO_WIPE_SPEED) \
			--resultsPath="$(SIM_RESULTS_PATH)/$(TIMEDATE_STR)/{}"

debug:
		$(NS3_BIN) run --gdb $(NS3_ADHOC_SIM_SRC)


