#!/usr/bin/env python
# coding: utf-8
#
#	Copyright 2015 Teemu Piippo
#	All rights reserved.
#
#	Redistribution and use in source and binary forms, with or without
#	modification, are permitted provided that the following conditions
#	are met:
#
#	1. Redistributions of source code must retain the above copyright
#	   notice, this list of conditions and the following disclaimer.
#	2. Redistributions in binary form must reproduce the above copyright
#	   notice, this list of conditions and the following disclaimer in the
#	   documentation and/or other materials provided with the distribution.
#	3. Neither the name of the copyright holder nor the names of its
#	   contributors may be used to endorse or promote products derived from
#	   this software without specific prior written permission.
#
#	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
#	TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
#	PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
#	OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#	EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO,
#	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#	PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING
#	NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

import re, hashlib

class OutputFile:
	'''
		Represents a file to write to. The write method actually writes to a buffer and actual I/O only
		takes place when save() is called. The file is only written if it has changed since last save.
		A checksum is written at the top of the file for the check used for whether it has changed or not.

		OutputFile supports sections, allowing finer control over the structure of the written file.
		The write() function takes an optional section parameter, which specifies which section to write
		to. If not given, the section supplied with setcurrentsection (0 by default) takes effect.
		Upon saving, the file is written one section at a time.
	'''

	def __init__(self, filename):
		self.filename = filename
		try:
			with open(self.filename, "r") as fp:
				self.oldsum = fp.readline().replace('\n', '').replace('// ', '')
		except IOError:
			self.oldsum = ''

		self.sections = []
		self.sectionnames = []
		self.sectionids = {}
		self.currentsection = 0

	def setsections(self, *args):
		self.sectionnames = []
		self.sectionids = {}

		while len(self.sections) < len(args):
			self.sections.append('')

		for i, arg in enumerate(args):
			self.sectionnames.append(arg)
			self.sectionids[arg] = i
			i += 1

	def getsectionid(self, name):
		try:
			return self.sectionids[name]
		except KeyError:
			return addsection(name)

	def addsection(self, name):
		self.sections.append('')
		self.sectionnames.append(name)
		return len(self.sections) - 1

	def setcurrentsection(self, num):
		self.currentsection = num

	def write(self, text, section = -1):
		if section == -1:
			section = self.currentsection

		while section >= len(self.sections):
			self.sections.append('')
			self.sectionnames.append('Unnamed section')

		self.sections[section] += text

	def save(self):
		#for i, section in enumerate(self.sections):
		#	comment = '// Section %d: %s\n' % (i, self.sectionnames[i])
		#	self.sections[i] = comment + self.sections[i]

		md5 = hashlib.md5()

		for section in self.sections:
			md5.update(section.encode())

		checksum = md5.hexdigest()

		if checksum == self.oldsum:
			#print('%s is up to date' % self.filename)
			return False

		with open(self.filename, "w") as fp:
			fp.write('// %s\n' % checksum)
			fp.write('// This file has been automatically generated. Do not edit by hand.\n')

			for section in self.sections:
				fp.write(section)

			print(self.filename, 'written')
			return True
