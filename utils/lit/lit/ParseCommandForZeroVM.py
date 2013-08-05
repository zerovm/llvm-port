#!/usr/bin/env python

import os
import struct
import unittest
import hashlib
import re

from lit.Util import which



def prepareTestForZeroVM(script):
    """
    Modifies script so that it will be run under ZeroVM
    1) All NaCl-compatible ELFs would be prepended with zvsh
    2) Every file argument would be prepended with @. Exceptions:
    files intended for stdin and stdout
    3) /dev/null would be converted to @null
    """
    def isFile(path):
        # first check most simple case: path is existing file
        if os.path.isfile(path):
            return True
        # path is non-existent file, try to guess if path is file
        # temp files usually have .tmp extension
        if '.tmp' in path:
            return True

    if not isinstance(script, basestring):
        raise Exception("Input is not a string.")

    # eject everything that should not be parsed, e.g. double-quoted text
    result, replacements = ejectStringArguments(script)
    zvsh_commands = []
    # split commands and start parsing
    for command in result.split('|'):
        if not command:
            zvsh_commands.append(command)
            continue
        zvsh_command = []
        last = ""
        isCommandParsed = False
        isNaClELF = False
        # split arguments
        for index, arg in enumerate(command.split(' ')):
            if not arg:
                zvsh_command.append(arg)
                continue
            # check if command is host  or zerovm elf
            if not isCommandParsed:
                try:
                # analyze file: read first 8 bytes, check if 8-th byte is equal to 0x7b (magic!)
                    isNaClELF = struct.unpack("@ihbb", open(which(arg)).read(8))[3] == 0x7B
                except Exception as e:
                    print "Warning! Couldn't parse '%s'. Check it manually. Command is '%s'" % (arg, script)
                if isNaClELF:
                    zvsh_command.append("zvsh %s" % arg)
                else:
                    zvsh_command.append("%s" % arg)
                isCommandParsed = True
                continue
            # check if arg is a file
            # index > 0 - disable command itself from parsing
            # previous arg should not be '<' - else it is stdin and shouldn't be parsed
            if isNaClELF and isFile(arg) and last != "<" and last != ">" and last != ">>":
                zvsh_command.append("@%s" % arg)
            else:
                zvsh_command.append(arg)
            last = arg

        # creating argument string
        command = " ".join(zvsh_command)
        # creating whole command string
        zvsh_commands.append("%s" % command)


    # chaining commands
    if len(zvsh_commands):
        result = '|'.join(zvsh_commands)

    # inject non-parsable text back into input string
    # replace /dev/null with @null
    return injectStringArguments(result, replacements).replace(r'/dev/null', r'@null')

quotesRE = re.compile(r'(\".+?\")')
def ejectStringArguments(input):
    """
    Replaces all "bla-bla-bla" with some one-word hash
    Example: command "hsda | dwa d+ | anything" -sd -G --help "some help"
    would be converted into command HASH1 -sd -G --help HASH2

    return converted string and dictionary HASH:replaced string for inverse
    conversion
    """
    replaces = dict()
    for match in quotesRE.findall(input):
        replaces[hashlib.md5(match).hexdigest()] = match
    for k, v in replaces.items():
        input = input.replace(v, k)
    return input, replaces
def injectStringArguments(input, arguments):
    """
    Replaces all HASH occurences int input and replaces them with
    arguments[HASH]

     Example: command HASH1 -sd -G --help HASH2 with arguments = {HASH1:"111", HASH2:"222"}
     would be converted to command "111" -sd -G --help "222"

     returns converted string
    """
    for k, v in arguments.items():
        input = input.replace(k, v)
    return input



class ParseStringForZeroVMTest(unittest.TestCase):
    def setUp(self):
        self.input_2_cmd = "/home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/opt < /home/zvm/git/llvm-nacl/test/Analysis/BasicAA/2003-02-26-AccessSizeTest.ll -basicaa -gvn -instcombine -S | /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/FileCheck /home/zvm/git/llvm-nacl/test/Analysis/BasicAA/2003-02-26-AccessSizeTest.ll"
        self.output_2_cmd = "zvsh /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/opt < /home/zvm/git/llvm-nacl/test/Analysis/BasicAA/2003-02-26-AccessSizeTest.ll -basicaa -gvn -instcombine -S | zvsh /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/FileCheck @/home/zvm/git/llvm-nacl/test/Analysis/BasicAA/2003-02-26-AccessSizeTest.ll"

        self.input_1_cmd = "/home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/opt < /home/zvm/git/llvm-nacl/test/Analysis/BasicAA/2003-02-26-AccessSizeTest.ll -basicaa -gvn -instcombine -S"
        self.output_1_cmd = "zvsh /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/opt < /home/zvm/git/llvm-nacl/test/Analysis/BasicAA/2003-02-26-AccessSizeTest.ll -basicaa -gvn -instcombine -S"

        self.input_with_stdin = self.input_2_cmd
        self.output_with_stdin = "zvsh /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/opt < /home/zvm/git/llvm-nacl/test/Analysis/BasicAA/2003-02-26-AccessSizeTest.ll -basicaa -gvn -instcombine -S | zvsh /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/FileCheck @/home/zvm/git/llvm-nacl/test/Analysis/BasicAA/2003-02-26-AccessSizeTest.ll"

        self.input_with_file = self.input_2_cmd
        self.output_with_file = "zvsh /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/opt < /home/zvm/git/llvm-nacl/test/Analysis/BasicAA/2003-02-26-AccessSizeTest.ll -basicaa -gvn -instcombine -S | zvsh /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/FileCheck @/home/zvm/git/llvm-nacl/test/Analysis/BasicAA/2003-02-26-AccessSizeTest.ll"

        self.input_with_stdout = "/home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/opt < /home/zvm/git/llvm-nacl/test/Analysis/ScalarEvolution/sext-inreg.ll -analyze -scalar-evolution > /home/zvm/git/llvm-nacl/build_debug/test/Analysis/ScalarEvolution/Output/sext-inreg.ll.tmp"
        self.output_with_stdout = "zvsh /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/opt < /home/zvm/git/llvm-nacl/test/Analysis/ScalarEvolution/sext-inreg.ll -analyze -scalar-evolution > /home/zvm/git/llvm-nacl/build_debug/test/Analysis/ScalarEvolution/Output/sext-inreg.ll.tmp"

        self.input_with_host = """grep "sext i59 {0,+,199}<%bb> to i64" /home/zvm/git/llvm-nacl/build_debug/test/Analysis/ScalarEvolution/Output/sext-inreg.ll.tmp | /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/count 1 /home/zvm/git/llvm-nacl/test/Analysis/ScalarEvolution/sext-inreg.ll"""
        self.output_with_host = """grep "sext i59 {0,+,199}<%bb> to i64" /home/zvm/git/llvm-nacl/build_debug/test/Analysis/ScalarEvolution/Output/sext-inreg.ll.tmp | zvsh /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/count 1 @/home/zvm/git/llvm-nacl/test/Analysis/ScalarEvolution/sext-inreg.ll"""

        self.input_with_whitespace = """/home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/opt < /home/zvm/git/llvm-nacl/test/Analysis/LoopInfo/2003-05-15-NestingProblem.ll -analyze -loops |    grep "^            Loop at depth 4 containing: %loopentry.7<header><latch><exiting>"""
        self.output_with_whitespace = """zvsh /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/opt < /home/zvm/git/llvm-nacl/test/Analysis/LoopInfo/2003-05-15-NestingProblem.ll -analyze -loops |    grep "^            Loop at depth 4 containing: %loopentry.7<header><latch><exiting>"""

        self.input_with_quotes = """/home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/llc < /home/zvm/git/llvm-nacl/test/CodeGen/X86/fp_load_fold.ll -march=x86 -x86-asm-syntax=intel |    grep -i ST "some1" | /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/not grep "fadd\\|fsub\\|fdiv\\|fmul" """
        self.output_with_quotes = """zvsh /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/llc < /home/zvm/git/llvm-nacl/test/CodeGen/X86/fp_load_fold.ll -march=x86 -x86-asm-syntax=intel |    grep -i ST "some1" | zvsh /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/not grep "fadd\\|fsub\\|fdiv\\|fmul" """

        self.input_codegen = """/home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/llc < /home/zvm/git/llvm-nacl/test/CodeGen/CPP/2009-05-04-CondBr.ll -march=cpp -cppgen=program -o /home/zvm/git/llvm-nacl/build_debug/test/CodeGen/CPP/Output/2009-05-04-CondBr.ll.tmp"""
        self.output_codegen = """zvsh /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/llc < /home/zvm/git/llvm-nacl/test/CodeGen/CPP/2009-05-04-CondBr.ll -march=cpp -cppgen=program -o @/home/zvm/git/llvm-nacl/build_debug/test/CodeGen/CPP/Output/2009-05-04-CondBr.ll.tmp"""

        self.input_with_dev_null = """/home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/llc < /home/zvm/git/llvm-nacl/test/CodeGen/X86/vec_shuffle-20.ll -o /dev/null -march=x86 -mcpu=corei7 -mtriple=i686-apple-darwin9 -stats -info-output-file - | grep asm-printer | grep 2"""
        self.output_with_dev_null = """zvsh /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/llc < /home/zvm/git/llvm-nacl/test/CodeGen/X86/vec_shuffle-20.ll -o @null -march=x86 -mcpu=corei7 -mtriple=i686-apple-darwin9 -stats -info-output-file - | grep asm-printer | grep 2"""

        self.input_with_append_stdout = """/home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/llc < /home/zvm/git/llvm-nacl/test/CodeGen/X86/vector.ll -march=x86 -mcpu=yonah >> /home/zvm/git/llvm-nacl/build_debug/test/CodeGen/X86/Output/vector.ll.tmp"""
        self.output_with_append_stdout = """zvsh /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/llc < /home/zvm/git/llvm-nacl/test/CodeGen/X86/vector.ll -march=x86 -mcpu=yonah >> /home/zvm/git/llvm-nacl/build_debug/test/CodeGen/X86/Output/vector.ll.tmp"""

    def test_parsingStdin(self):
        self.assertEqual(self.output_with_stdin, prepareTestForZeroVM(self.input_with_stdin))
    def test_chaining2Commands(self):
        self.assertEqual(self.output_2_cmd, prepareTestForZeroVM(self.input_2_cmd))
    def test_oneCommand(self):
        self.assertEqual(self.output_1_cmd, prepareTestForZeroVM(self.input_1_cmd))
    def test_input(self):
        self.assertEqual("", prepareTestForZeroVM(""))
        self.assertRaises(Exception, prepareTestForZeroVM, 1)
        self.assertRaises(Exception, prepareTestForZeroVM, 1.0)
        self.assertRaises(Exception, prepareTestForZeroVM, [])
        self.assertRaises(Exception, prepareTestForZeroVM, {})
    def test_parsingFiles(self):
        self.assertEqual(self.output_with_file, prepareTestForZeroVM(self.input_with_file))
    def test_parseStdout(self):
        self.assertEqual(self.output_with_stdout, prepareTestForZeroVM(self.input_with_stdout))
    def test_parseWithHost(self):
        self.assertEqual(self.output_with_host, prepareTestForZeroVM(self.input_with_host))
    def test_parseWhitespace(self):
        self.assertEqual(self.output_with_whitespace, prepareTestForZeroVM(self.input_with_whitespace))
    def test_parseWithQuotes(self):
        self.assertEqual(self.output_with_quotes, prepareTestForZeroVM(self.input_with_quotes))
    def test_parseCodegen(self):
        self.assertEqual(self.output_codegen, prepareTestForZeroVM(self.input_codegen))
    def test_parseWithDevNull(self):
        self.assertEqual(self.output_with_dev_null, prepareTestForZeroVM(self.input_with_dev_null))
    def test_parseWithAppendStdout(self):
        self.assertEqual(self.output_with_append_stdout, prepareTestForZeroVM(self.input_with_append_stdout))

class ProcessStringArgumentsTest(unittest.TestCase):
    def setUp(self):
        self.grep_input = """/home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/llc < /home/zvm/git/llvm-nacl/test/CodeGen/X86/fp_load_fold.ll -march=x86 -x86-asm-syntax=intel |    grep -i ST | /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/not grep "fadd\\|fsub\\|fdiv\\|fmul" """

        self.two_grep_input = """/home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/llc < /home/zvm/git/llvm-nacl/test/CodeGen/X86/fp_load_fold.ll -march=x86 -x86-asm-syntax=intel |    grep -i ST "some" | /home/zvm/git/llvm-nacl/build_debug/Debug+Asserts/bin/not grep "fadd\\|fsub\\|fdiv\\|fmul" """

    def test_removeQuotes(self):
        self.assertIn('"', self.grep_input)
        self.assertNotIn('"', ejectStringArguments(self.grep_input)[0])
        self.assertIsNotNone(ejectStringArguments(self.grep_input)[1])

    def test_complimentarity(self):
        self.assertEqual(self.grep_input,
                         injectStringArguments(*ejectStringArguments(self.grep_input)))

    def test_twoReplacement(self):
        self.assertEqual(len(ejectStringArguments(self.two_grep_input)[1]), 2)
        self.assertEqual(self.two_grep_input,
                         injectStringArguments(*ejectStringArguments(self.two_grep_input)))
