FILAMENTS_FILE ?= ./tools/tmp/bambu_lab_filaments.json
SPOOLMAN_VENDOR ?= Bambu Lab

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

import_filaments:
	@test -n "$(SPOOLMAN_URL)" || (echo "SPOOLMAN_URL is required. Example: make import_filaments SPOOLMAN_URL=http://localhost:7912" && exit 1)
	node ./tools/import_bambu_lab_filaments_to_spoolman.js --url "$(SPOOLMAN_URL)" --trace --dry-run

sync_filaments:
	$(MAKE) fetch_filaments
	@echo ""
	$(MAKE) import_filaments SPOOLMAN_URL="$(SPOOLMAN_URL)" FILAMENTS_FILE="$(FILAMENTS_FILE)" SPOOLMAN_VENDOR="$(SPOOLMAN_VENDOR)"
