all: connect-usb
PACKAGE = connect-usb-1.3

connect-usb: connect-usb.c
	gcc -g -o connect-usb connect-usb.c -lc -Wall

dist:
	mkdir $(PACKAGE)
	cp *.c $(PACKAGE)
	cp *.conf $(PACKAGE)
	cp -r samples $(PACKAGE)
	cp Makefile $(PACKAGE)
	tar czfv $(PACKAGE).tar.gz $(PACKAGE)
	rm -r $(PACKAGE)

clean:
	rm connect-usb
