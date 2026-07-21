INFORM = inform
OZMOO = /home/johan/commodore/ozmoo-z6
PUNY = /home/johan/commodore/punyinform
X16 = /home/johan/commodore/ozmoo/x16-emulator46/x16emu
XMEGA65 = xemu-xmega65
WYRMWARD = /home/johan/commodore/punyinform/Wyrmward

all: test

z5:
	$(INFORM) +$(PUNY)/lib -v5 -es -D wyrmward.inf

blorb:
	python $(OZMOO)/tools/make_blorb.py resources

z6: blorb
	$(INFORM) +$(PUNY)/lib -v6 -es -D wyrmward.inf

x16: blorb z6
	cd $(OZMOO) && ruby make.rb -t:x16 -pics $(WYRMWARD)/wyrmward.blb  $(WYRMWARD)/wyrmward.z6 && cp x16_wyrmward.zip $(WYRMWARD) && cd $(WYRMWARD) && rm -rf x16_wyrmward && unzip x16_wyrmward.zip && cd x16_wyrmward && $(X16) -prg WYRMWARD.PRG -run

mega65: blorb z6
	cd $(OZMOO) && ruby make.rb -t:mega65 -fcm -pics $(WYRMWARD)/wyrmward.blb  $(WYRMWARD)/wyrmward.z6 && cp mega65_wyrmward.d81 $(WYRMWARD) && cd $(WYRMWARD) && $(XMEGA65) -8 mega65_wyrmward.d81

test:z5
	rm -f wyrmward.scr wyrmward.qzl
	frotz wyrmward.z5 < wyrmward.cmd
	meld wyrmward.scr wyrmward.txt

frotz: z5
	frotz -d wyrmward.z5

# --xscale 2 --yscale 2 is not optional here: sfrotz's screen is
# always 640x400 and it draws pictures at 1:1 unless it detects 
# that the game is one of ARTHUR/JOURNEY/SHOGUN/ZORK_ZERO
sfrotz: z6
	sfrotz --xscale 2 --yscale 2 wyrmward.z6

release:
	$(INFORM) +$(PUNY)/lib -v5 -es wyrmward.inf
	frotz -d wyrmward.z5

clean:
	rm -rf wyrmward.z5 wyrmward.z6 wyrmward.blb wyrmward.scr pics *.d81 x16_wyrmward*
