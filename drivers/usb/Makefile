#	 Upanix - An x86 based Operating System
#	 Copyright (C) 2011 'Prajwala Prabhakar' 'srinivasa_prajwal@yahoo.co.in'
#																			 
#	 This program is free software: you can redistribute it and/or modify
#	 it under the terms of the GNU General Public License as published by
#	 the Free Software Foundation, either version 3 of the License, or
#	 (at your option) any later version.
#																			 
#	 This program is distributed in the hope that it will be useful,
#	 but WITHOUT ANY WARRANTY; without even the implied warranty of
#	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	 GNU General Public License for more details.
#																			 
#	 You should have received a copy of the GNU General Public License
#	 along with this program.  If not, see <http://www.gnu.org/licenses/
DEPS := $(shell $(UPANIX_HOME)/depend.sh)
OBJS := $(patsubst %.cpp,%.o,$(wildcard *.cpp))

build: ${OBJS}
	cd uhci && make
	cd ehci && make
	cd xhci && make
	cd disk && make

include depend.d

clean:
	rm -f *.o
	cd uhci && make clean
	cd ehci && make clean
	cd xhci && make clean
	cd disk && make clean

distclean:
	rm -f *.o
	cd uhci && make distclean
	cd ehci && make distclean
	cd xhci && make distclean
	cd disk && make distclean
