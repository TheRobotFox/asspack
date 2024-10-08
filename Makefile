##
# AssPack
#
# @file
# @version 0.1

DEPFILE ?= #../dependencies
ASSETS ?= #../assets

res: resPack Packer.cpp
	ld -r -b binary resPack -o $@

resPack: packer $(DEPFILE)
	./packer $(DEPFILE) $(ASSETS) $@

packer: Packer.cpp
	$(CXX) -o $@ $< -std=c++26 -ggdb
.PHONY clean
clean:
	rm -f packer res resPack


# end
