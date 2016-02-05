
default: all

.DEFAULT:
	cd src && make $@

install:
	cd src && make $@

.PHONY: clean

clean:
	cd src && make $@
