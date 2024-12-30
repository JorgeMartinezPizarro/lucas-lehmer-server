PHONY: build push start

DOCKER_USER=jorgemartinezpizarro

build:
	cd go && docker build -t ${DOCKER_USER}/lucas-lehmer-server:go .
push:
	docker push ${DOCKER_USER}/lucas-lehmer-server:go
start:
	cd go && docker compose up -d
stop:
	cd go && docker compose down --remove-orphans
