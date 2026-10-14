.PHONY: build push start stop test tools estimate scale

export DOCKER_USER ?= jorgemartinezpizarro

# c/test_llt, built in the build stage of the C image
TOOLS = lucas-lehmer-server:c-tools
P ?= 6972593
THREADS ?= 1 2 4 8

build:
	docker compose build
push:
	docker compose push
start:
	docker compose up -d
stop:
	docker compose down --remove-orphans
test:
	python3 scripts/smoke_test.py

tools:
	docker build -q --target build -t $(TOOLS) c > /dev/null
# Time a full test of each exponent on one core: make estimate P="6972593 13466917"
estimate: tools
	docker run --rm $(TOOLS) ./test_llt estimate $(P)
# Speed of each exponent on several threads: make scale P=6972593 THREADS="1 2 4 8 16 20"
scale: tools
	for p in $(P); do docker run --rm $(TOOLS) ./test_llt scale $$p $(THREADS); done
