PHONY: build push start

DOCKER_USER=jorgemartinezpizarro

build:
	cd go && docker build -t ${DOCKER_USER}/lucas-lehmer-server:go .
	cd fortran && docker build -t ${DOCKER_USER}/lucas-lehmer-server:fortran .
push:
	docker push ${DOCKER_USER}/lucas-lehmer-server:go
	docker push ${DOCKER_USER}/lucas-lehmer-server:fortran
start:
	docker compose up -d
stop:
	docker compose down --remove-orphans
