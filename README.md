# Itty

Itty is an interpreter and virtual environment for the
[BIT](http://www.dangermouse.net/esoteric/bit.html)
programming language. The main advantage of BIT is its unprecedented
level of control over the representation of data in memory, and Itty
augments this by virtualising the BIT memory system. Your program sees
complex, fine-grained bit-level storage, and is seamlessly shielded from
the easy-to-understand abstract data structures managed behind the
scenes by the interpreter. This allows the effortless bitwise operation
(NAND) that BIT is famous for while addressing the practical concerns of
program speed and memory efficiency which might otherwise be too good to
allow the sale of optimised updates for BIT programs.

## Building

`g++ itty.cpp -o itty -std=c++17`

## Usage

`./itty [--strict] PROGRAM`

## Standards Compliance

Itty strives to allow well-tested legacy BIT applications to continue
powering real-world solutions until the end of time. However, this often
creates difficulty in fully adhering to the language specification.
According to the BIT specification, "the value of the jump register may
not be assigned to a variable using the EQUALS operator". However, the
existing programs "Bit Addition" and "Repeat Arbitrary Number of Ones"
use this invalid construct. For this reason, by default, Itty allows
this but will display a warning at parse time. If full compliance with
the BIT specification is required, Itty can be invoked with the
`--strict` parameter, which will cause this construct to create an
error.
