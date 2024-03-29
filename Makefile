all: connect-usb
PACKAGE = connect-usb-1.7
prefix ?= /usr/local/bin


connect-usb: connect-usb.c usb-serials.c
	gcc -g -o connect-usb connect-usb.c usb-serials.c -lc -Wall

dist:
	mkdir $(PACKAGE)
	cp *.c $(PACKAGE)
	cp *.conf $(PACKAGE)
	cp -r samples $(PACKAGE)
	cp Makefile $(PACKAGE)
	tar czfv $(PACKAGE).tar.gz $(PACKAGE)
	rm -r $(PACKAGE)

install: connect-usb
	cp connect-usb $(prefix)/

clean:
	rm connect-usb
