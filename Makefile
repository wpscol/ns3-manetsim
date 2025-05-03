-include .env.example
-include .env

default: init

init: cpenv link configure build

download:
	wget 'https://www.nsnam.org/releases/ns-allinone-3.44.tar.bz2'
	tar xvf ns-allinone-3.44.tar.bz2
	rm ns-allinone-3.44.tar.bz2

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

run:
	$(NS3_DIR)/$(NS3_ADHOC_SIM_BIN) \
		--nodesNum=$(SIM_NODES_NUM) \
		--areaSizeX=$(SIM_AREA_SIZE_X) \
		--areaSizeY=$(SIM_AREA_SIZE_Y) \
		--resultsPath=$(SIM_RESULTS_PATH)/$(shell date +"%H-%M_%d-%m-%Y") \
		--simulationTime=$(SIM_TIME) \
		--warmupTime=$(SIM_WARMUP_TIME) \

