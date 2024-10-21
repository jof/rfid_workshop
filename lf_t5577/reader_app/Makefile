.PHONY: all
all: requirements.txt requirements-dev.txt

%.txt: %.in
	pip-compile -o $@ $<

requirements-dev.txt: requirements.txt

.PHONY: clean
clean:
	rm -f *.txt

.PHONY: install
install:
	pip-sync requirements.txt requirements-dev.txt

.PHONY: link_pm3
link_pm3:
	ln -s ~/src/proxmark3/client/experimental_lib/build/libpm3rrg_rdv4.so ./_pm3.so
