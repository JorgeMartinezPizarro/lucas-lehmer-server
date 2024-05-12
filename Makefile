PHONY: build push start

DOCKER_USER=jorgemartinezpizarro
BACKUP_NAME=backup
DATE=`date +'%d-%m-%Y'`

## Export for the Dockerfile.
export SERVER_PATH=/root/repositories/lucas-lehmer-server

build:
	## Build go
	cd ${SERVER_PATH}/go/ && docker build -t ${DOCKER_USER}/lucas-lehmer-go-server:latest .
	## Build scala
	cd ${SERVER_PATH}/scala/ && docker build -t ${DOCKER_USER}/lucas-lehmer-scala-server:latest .
push:
	## Push go
	cd ${SERVER_PATH}/ && docker push ${DOCKER_USER}/lucas-lehmer-go-server:latest
	## Push scala
	cd ${SERVER_PATH}/ && docker push ${DOCKER_USER}/lucas-lehmer-scala-server:latest
start:
	## Restart go and scala servers
	cd ${SERVER_PATH}/ && docker compose down --remove-orphans && docker compose up -d
