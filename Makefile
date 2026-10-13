.PHONY: build push start stop test

export DOCKER_USER ?= jorgemartinezpizarro

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
