-include .env.example
-include .env

TIMEDATE_STR := $(shell date +"%H-%M_%d-%m-%Y")

VENV_PATH := ./.venv
PYTHON_BIN := $(VENV_PATH)/bin/python3

RAND_VAL := $(shell echo $$RANDOM)

default: init

init: cpenv download rmdefault link configure venv

run: run_ns3 plot

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

plot:
	echo $(SIM_RNG_RUNS) | tr ' ' '\n' | /usr/bin/parallel \
		$(PYTHON_BIN) ./scripts/plot-movement.py \
			--x_max=$(SIM_AREA_SIZE_X) \
			--y_max=$(SIM_AREA_SIZE_Y) \
			--input_path="$(SIM_RESULTS_PATH)/$(TIMEDATE_STR)/{}/movement.csv" \
			--output_path="$(SIM_RESULTS_PATH)/$(TIMEDATE_STR)/{}/movement_plot.png"

run_ns3:
	echo $(SIM_RNG_RUNS) | tr ' ' '\n' | /usr/bin/parallel \
		$(NS3_DIR)/$(NS3_ADHOC_SIM_BIN) \
			--rngRun="{}" \
			--rngSeed=$(SIM_RNG_SEED) \
			--nodesNum=$(SIM_NODES_NUM) \
			--areaSizeX=$(SIM_AREA_SIZE_X) \
			--areaSizeY=$(SIM_AREA_SIZE_Y) \
			--resultsPath="$(SIM_RESULTS_PATH)/$(TIMEDATE_STR)/{}" \
			--simulationTime=$(SIM_TIME) \
			--warmupTime=$(SIM_WARMUP_TIME) \
			--samplingFreq=$(SIM_SAMPLING_FREQ)
