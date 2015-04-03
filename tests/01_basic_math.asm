;; Copyright (C) 2014-2015 Ben Kurtovic <ben.kurtovic@gmail.com>
;; Released under the terms of the MIT License. See LICENSE for details.

; ----- CRATER UNIT TESTING SUITE ---------------------------------------------

; This file contains test cases for basic arithmetic operations on registers,
; without involving memory or control flow.

.include	"_header.asm"

test:
	ld	a, 0
	ld	b, 0
	ld	c, 0
	ld	d, 0
	ld	e, 0
	ld	h, 0
	ld	l, 0
	emu	rassert(a=$00, b=$00, c=$00, d=$00, e=$00, h=$00, l=$00)

	inc	a
	emu	rassert(a=$01, b=$00, c=$00, d=$00, e=$00, h=$00, l=$00)
	emu	fassert(s=0, z=0, f5=0, h=0, f3=0, pv=0, n=0)

	inc	b
	emu	rassert(a=$01, b=$01, c=$00, d=$00, e=$00, h=$00, l=$00)
	emu	fassert(s=0, z=0, f5=0, h=0, f3=0, pv=0, n=0)

	inc	c
	emu	rassert(a=$01, b=$01, c=$01, d=$00, e=$00, h=$00, l=$00)
	emu	fassert(s=0, z=0, f5=0, h=0, f3=0, pv=0, n=0)

	inc	d
	emu	rassert(a=$01, b=$01, c=$01, d=$01, e=$00, h=$00, l=$00)
	emu	fassert(s=0, z=0, f5=0, h=0, f3=0, pv=0, n=0)

	inc	e
	emu	rassert(a=$01, b=$01, c=$01, d=$01, e=$01, h=$00, l=$00)
	emu	fassert(s=0, z=0, f5=0, h=0, f3=0, pv=0, n=0)

	inc	h
	emu	rassert(a=$01, b=$01, c=$01, d=$01, e=$01, h=$01, l=$00)
	emu	fassert(s=0, z=0, f5=0, h=0, f3=0, pv=0, n=0)

	inc	l
	emu	rassert(a=$01, b=$01, c=$01, d=$01, e=$01, h=$01, l=$01)
	emu	fassert(s=0, z=0, f5=0, h=0, f3=0, pv=0, n=0)
