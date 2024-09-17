HP8970 Companion
================

Michael G. Katzmann (NV3Z/VK2BEA/G4NYV)
------------------------------------------------------------------

Program to augment functionality of the HP 8970 Noise Figure Meter:
1. Control the 8970 via GPIB to take measurements of noise figure and gain over a frequency sweep or over time.
2. Plot the noise figure/temperature and gain vs frequency
       - Use mouse to examine the trace (interrogate plot to see measurements and interpolated values)
3. Save traces as an image file (PNG) or PDF.
4. Save measured data as CSV for further processing or as a JSON file to be retrieved at a later time.
5. Print traces to connected printer

![image](https://github.com/user-attachments/assets/3bf89ff2-d9ea-44f1-8c72-5111815e0e86)

A Fedora RPM package is available, install the program and GPIB driver, GPIB utilities and firmware by:
-------------------------------------------------------------------------------------------------------
`sudo dnf copr enable vk2bea/GPIB`  
`sudo dnf copr enable vk2bea/HP8970`  
`sudo dnf install dkms-linux-gpib linux-gpib linux-gpib-devel linux-gpib-firmware hp8970`

To build & install using Linux autotools, install the following required packages & tools:
------------------------------------------------------------------------------------------
* `automake`, `autoconf` and `libtool`  
* To build on Raspberri Pi / Debian: 	`libgs-dev, libglib2.0-dev, libgtk-4-dev, librsvg2-dev, libjson-c-dev, `  
                `libjson-glib-dev, yelp-tools, https://linux-gpib.sourceforge.io/`  
* To run on Raspberry Pi / Debian :	`libglib-2, libgtk-4, librsvg2-2, libgpib, json-c, libjson-c-dev, libjson-glib-dev, json-glib`

** Important: You need GTK4 Version 4.12 or higher to compile and run this program

Install the GPIB driver: 
Visit https://linux-gpib.sourceforge.io/ for installation instructions.

The National Instruments GPIB driver *may* also be used, but this has not been tested. The Linux GPIB API is compatable with the NI library.... quote: *"The API of the C library is intended to be compatible with National Instrument's GPIB library."*

Once the prerequisites (as listed above) are installed, install the 'HP8970 Companion' with these commands:

        $ ./autogen.sh
        $ cd build/
        $ ../configure
        $ make all
        $ sudo make install
To run:
        
        $ /usr/local/bin/hp8970 

To uninstall:
        
        $ sudo make uninstall

Troubleshooting:
----------------------------------------------------------------------
If problems are encountered, first confirm that correct GPIB communication is occurring. 
<br/>__Warning__ *You must setup the GPIB controller and the GPIB address of the 8970 on the GPIB tab of the GUI. (press the ▶ icon on the notebook widget until the GPIB tab is shown, or right click on any tab and select GPIB from the list)*

Use the `ibtest` and `ibterm` tools distributed with the `linux-gpib` distribution.

The HP8970 Companion logs some information to the journal, the verbosity of which can be set with the `--debug` command line switch.

To enable debugging output to the journal, start the program with the `--debug 7` switch, <em>(Debug levels 0-7)</em>.

If started without the switch, the default logging verbosity is 3.

To view the output (in follow mode) use:

        journalctl -t hp8970 -f
