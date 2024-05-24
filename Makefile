PHONY: build push start

DOCKER_USER=jorgemartinezpizarro
BACKUP_NAME=backup
DATE=`date +'%d-%m-%Y'`

## Export for the Dockerfile.
export SERVER_PATH=/root/repositories/lucas-lehmer-server

build:
	## Build rust
	cd ${SERVER_PATH}/rust/ && docker build -t ${DOCKER_USER}/lucas-lehmer-server:rust .
	## Build go
	cd ${SERVER_PATH}/go/ && docker build -t ${DOCKER_USER}/lucas-lehmer-server:go .
	## Build scala
	cd ${SERVER_PATH}/scala/ && docker build -t ${DOCKER_USER}/lucas-lehmer-server:scala .
	## Build scala
	cd ${SERVER_PATH}/c/ && docker build -t ${DOCKER_USER}/lucas-lehmer-server:c .
push:
	## Push rust
	cd ${SERVER_PATH}/ && docker push ${DOCKER_USER}/lucas-lehmer-server:rust
	## Push go
	cd ${SERVER_PATH}/ && docker push ${DOCKER_USER}/lucas-lehmer-server:go
	## Push scala
	cd ${SERVER_PATH}/ && docker push ${DOCKER_USER}/lucas-lehmer-server:scala
	## Push scala
	cd ${SERVER_PATH}/ && docker push ${DOCKER_USER}/lucas-lehmer-server:c
start:
	## Restart go and scala servers
	cd ${SERVER_PATH}/ && docker compose down --remove-orphans && docker compose up -d
