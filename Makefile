INFORM = inform
OZMOO = /home/johan/commodore/ozmoo-z6
PUNY = /home/johan/commodore/punyinform
X16 = /home/johan/commodore/ozmoo/x16-emulator46/x16emu
XMEGA65 = xemu-xmega65

# make.rb anchors everything it reads to its own directory (asm/, tools/, temp/,
# exomizer) and writes the finished disk image into the CURRENT directory, so it
# can be called from here with plain relative paths -- no cd into $(OZMOO), no
# copying the image back. It also leaves the X16 build unpacked in
# x16_wyrmward/ beside the zip, so there is nothing to unzip either.
OZMOOBUILD = ruby $(OZMOO)/make.rb

# What each generated file is actually made from. The blorb holds the PICTURES
# only -- the sound effects go on the disk straight from resources/ via -asw --
# so the wavs are a prerequisite of the MEGA65 image, not of the blorb, and the
# X16 build (no sound) needs neither.
PICSRC   = resources/contents.yaml $(wildcard resources/*.png)
STORYSRC = wyrmward.inf ext_z6graphics.h $(wildcard $(PUNY)/lib/*.h)
WAVS     = resources/003.wav resources/004.wav

all: test

z5:
	$(INFORM) +$(PUNY)/lib -v5 -es -D wyrmward.inf

# Real file rules, not one recipe under a phony name: sox dithers with a fresh
# random seed on every run, so remaking these unconditionally rewrote both wavs
# bit-for-bit differently each time, and the blorb and the disk image with them,
# even when nothing had changed. Now sox runs only when the source wav is newer.
sounds/hell_8k.wav: sounds/hell.wav
	sox $< -r 8000 $@

sounds/intro_6k.wav: sounds/intro.wav
	sox $< -r 6000 $@

resources/004.wav: sounds/hell_8k.wav
	cp $< $@

resources/003.wav: sounds/intro_6k.wav
	cp $< $@

sound: $(WAVS)

wyrmward.blb: $(PICSRC)
	python $(OZMOO)/tools/make_blorb.py resources

blorb: wyrmward.blb

wyrmward.z6: $(STORYSRC)
	$(INFORM) +$(PUNY)/lib -v6 -es -D wyrmward.inf

z6: wyrmward.z6

x16_wyrmward.zip: wyrmward.blb wyrmward.z6
	$(OZMOOBUILD) -t:x16 -pics wyrmward.blb wyrmward.z6

x16: x16_wyrmward.zip
	# the emulator must run from inside the game directory
	cd x16_wyrmward && $(X16) -prg WYRMWARD.PRG -run

mega65_wyrmward.d81: wyrmward.blb wyrmward.z6 $(WAVS)
	$(OZMOOBUILD) -t:mega65 -asw resources -fcm -pics wyrmward.blb wyrmward.z6

# SDL_AUDIODRIVER is not optional: SDL does not get on with pipewire on
# Fedora/KDE, and xemu then comes up silent with no error at all.
mega65: mega65_wyrmward.d81
	SDL_AUDIODRIVER=pulseaudio $(XMEGA65) -8 mega65_wyrmward.d81

.PHONY: all z5 sound blorb z6 x16 mega65 test frotz sfrotz release clean

test:z5
	rm -f wyrmward.scr wyrmward.qzl
	frotz wyrmward.z5 < wyrmward.cmd
	meld wyrmward.scr wyrmward.txt

frotz: z5
	frotz -d wyrmward.z5

# --xscale 2 --yscale 2 is not optional here: sfrotz's screen is
# always 640x400 and it draws pictures at 1:1 unless it detects
# that the game is one of ARTHUR/JOURNEY/SHOGUN/ZORK_ZERO
#
# SDL_AUDIODRIVER for the same reason as the MEGA65 target: sfrotz plays the
# Blorb's sound effects, and without this it comes up silent on Fedora/KDE.
sfrotz: wyrmward.z6 wyrmward.blb
	SDL_AUDIODRIVER=pulseaudio sfrotz --xscale 2 --yscale 2 wyrmward.z6

release:
	$(INFORM) +$(PUNY)/lib -v5 -es wyrmward.inf
	frotz -d wyrmward.z5

clean:
	rm -rf wyrmward.z5 wyrmward.z6 wyrmward.blb wyrmward.scr pics *.d81 x16_wyrmward* *qzl
