-include .env.example
-include .env

TIMEDATE_STR := $(shell date +"%H-%M_%d-%m-%Y")

VENV_PATH := ./.venv
PYTHON_BIN := $(VENV_PATH)/bin/python3

RAND_VAL := $(shell echo $$RANDOM)

default: init

init: cpenv link configure build venv

download:
	wget 'https://www.nsnam.org/releases/ns-allinone-3.44.tar.bz2'
	tar xvf ns-allinone-3.44.tar.bz2
	rm ns-allinone-3.44.tar.bz2

venv:
	/usr/bin/python3 -m venv .venv
	$(PYTHON_BIN) -m pip install \
		pandas \
		matplotlib \
		black

plot:
	$(PYTHON_BIN) ./scripts/plot-movement.py \
		--x_max=$(SIM_AREA_SIZE_X) \
		--y_max=$(SIM_AREA_SIZE_Y) \
		$(SIM_RESULTS_PATH)/$(TIMEDATE_STR)/movement_plot.png \
		$(SIM_RESULTS_PATH)/$(TIMEDATE_STR)/movement.csv

clean:
	rm -rf $(NS3_DIR)/build
	rm -rf $(NS3_DIR)/cmake-cache
	rm -rf $(NS3_DIR)/scratch

configure:
	$(NS3_BIN) configure

build:
	$(NS3_BIN) build -j$(shell nproc) $(NS3_ADHOC_SIM_SRC)

cpenv:
	cp -f --update=none .env.example .env

link:
	ln -sfn $(shell pwd)/scratch/ $(NS3_DIR)/scratch/

run_ns3:
	$(NS3_DIR)/$(NS3_ADHOC_SIM_BIN) \
		--RngRun=$(RAND_VAL) \
		--nodesNum=$(SIM_NODES_NUM) \
		--areaSizeX=$(SIM_AREA_SIZE_X) \
		--areaSizeY=$(SIM_AREA_SIZE_Y) \
		--resultsPath=$(SIM_RESULTS_PATH)/$(TIMEDATE_STR) \
		--simulationTime=$(SIM_TIME) \
		--warmupTime=$(SIM_WARMUP_TIME) \
		--samplingFreq=$(SIM_SAMPLING_FREQ)

run: run_ns3 plot
