PHONY: build push start

DOCKER_USER=jorgemartinezpizarro
BACKUP_NAME=backup
DATE=`date +'%d-%m-%Y'`

## Export for the Dockerfile.
export SERVER_PATH=/root/repositories/lucas-lehmer-server

build:
	## Build go
	cd ${SERVER_PATH}/go/ && docker build -t ${DOCKER_USER}/lucas-lehmer-go-server:latest .
	## Build python
	cd ${SERVER_PATH}/python/ && docker build -t ${DOCKER_USER}/lucas-lehmer-python-server:latest .
	## Build scala
	cd ${SERVER_PATH}/scala/ && docker build -t ${DOCKER_USER}/lucas-lehmer-scala-server:latest .
push:
	cd ${SERVER_PATH}/ && docker push ${DOCKER_USER}/lucas-lehmer-go-server:latest
	cd ${SERVER_PATH}/ && docker push ${DOCKER_USER}/lucas-lehmer-scala-server:latest
	cd ${SERVER_PATH}/ && docker push ${DOCKER_USER}/lucas-lehmer-python-server:latest
start:
	cd ${SERVER_PATH}/ && docker compose down --remove-orphans && docker compose up -d
