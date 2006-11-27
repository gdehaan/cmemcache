#
# $Id$
#

.PHONY: all
all: index.html

index.html: README
	rst2html.py README index.html

.PHONY: runtests
runtests: all
	python test.py
