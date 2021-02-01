libmcp23017 - A C library to drive the mcp23017

The mcp23017 is a 16-bit I/O expander with an I2C interface


Building
========
This project has a hard dependency on the i2c-tools library:
	https://i2c.wiki.kernel.org/index.php/I2C_Tools

This project uses the autoconf build system. After a fresh clone remember
to run:
```
	$ autoreconf -i
```

I generally build all my code/images with OpenEmbedded/Yocto, therefore my
recipes take care of these details.

NOTE: if building with an SDK, to do a _make distcheck_ (and your build host
is x86\_64), use:
```
	$ DISTCHECK_CONFIGURE_FLAGS=--host=x86_64 make distcheck
```


Samples
=======
The samples/ subdirectory contains a "doodle" app that was written as part
of the development of this library.

	samples/mcp23017.c:
		A stand-alone program that presents the user with a menu of
		commands to run. This program assumes the mcp23017 is wired to
		a RaspberryPi and that GPIO#4 is wired to the mcp23017's /RESET
		pin.

		Menu
		^^^^
		 0  - exit
		 1  - set gpio A
		 2  - set gpio B
		 3  - get gpio A
		 4  - get gpio B
		 5  - read register
		 9  - reset

	samples/mcp23017util.c
		Has, minimally, the same functionality as samples/mcp23017.c,
		except this one uses the in-progress libmcp23017.la.
		Also has extra menu items for set/clear individual bits.
		Specify the bits as: GPB6 or GPA2, etc…

		Menu
		^^^^
		 0  - exit
		 1  - set gpio A
		 2  - set gpio B
		 3  - get gpio A
		 4  - get gpio B
		 5  - read register
		 6  - set bit
		 7  - clear bit
		 9  - reset


Contributing
============
Please submit any issues or pull requests via the normal github workflow.
