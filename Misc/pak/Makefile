INPUT := gfx/conback.lmp \
	gfx/menumods.lmp \
	gfx/skillmenu.lmp \
	gfx/sp_maps.lmp \
	default.cfg

OUTPUT := ironwail.pak

$(OUTPUT): $(INPUT)
	./mkpak.sh $(INPUT) > $(OUTPUT)

.PHONY: clean
clean:
	rm -f $(OUTPUT)

