
default: all

.DEFAULT:
	cd src && make $@

install:
	cd src && make $@

test:
	cd src && make
	cd tests && make $@

.PHONY: clean test

clean:
	cd src && make $@
	cd tests && make $@
