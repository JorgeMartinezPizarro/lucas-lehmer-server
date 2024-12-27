PHONY: build push start

DOCKER_USER=jorgemartinezpizarro
BACKUP_NAME=backup
DATE=`date +'%d-%m-%Y'`

## Export for the Dockerfile.
export SERVER_PATH=/root/repositories/lucas-lehmer-server

build:
	## Build go
	cd ${SERVER_PATH}/go/ && docker build -t ${DOCKER_USER}/lucas-lehmer-server:go .
push:
	## Push go
	cd ${SERVER_PATH}/ && docker push ${DOCKER_USER}/lucas-lehmer-server:go
start:
	## Restart go and scala servers
	cd ${SERVER_PATH}/ && docker compose down --remove-orphans && docker compose up -d
