INPUT := gfx/conback.lmp \
	default.cfg

OUTPUT := ironwail.pak

$(OUTPUT): $(INPUT)
	./mkpak.sh $(INPUT) > $(OUTPUT)

.PHONY: clean
clean:
	rm -f $(OUTPUT)

