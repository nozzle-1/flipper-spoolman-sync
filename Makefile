cli:
	cd ./src && ufbt cli
clean:
	cd ./src && ufbt -c
build:
	make clean
	cd ./src && ufbt build
dev:
	make clean
	cd ./src && ufbt launch