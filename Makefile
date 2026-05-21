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

fetch_filaments:
	node ./tools/generate_bambu_lab_filaments_file.js