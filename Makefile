
default: all

.DEFAULT:
	cd src && make $@

install:
	cd src && make $@

test:
	@cd src && make
	@cd tests && ./run-tests.sh

test-ci:
	@cd tests && ./run-tests-ci.sh

.PHONY: clean test test-ci

clean:
	cd src && make $@
	cd tests && make $@
