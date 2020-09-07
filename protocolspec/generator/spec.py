#!/usr/bin/env python3
# coding: utf-8
#
#	Copyright 2016 Teemu Piippo
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

import re
import parametertypes
from collections import defaultdict, OrderedDict

def catches(function, exceptionType = Exception):
	'''
		Returns whether or not calling the provided function yields an exception of type exceptionType, or a descendant
		of it.
	'''
	try:
		function()
		return False
	except exceptionType:
		return True
	except Exception:
		return False

class NetworkSpec:
	'''
	Models the protocol specification.
	'''
	def __init__(self):
		self.currentcommand = None
		self.currentprotocol = None
		self.currentstruct = None
		self.protocols = {}
		self.commands = []
		self.commandsByName = {}
		self.includes = []
		self.loadstack = 0

	def findStructByName(self, name):
		'''
		Finds a struct by name, or None if none found.
		'''
		try:
			return self.currentprotocol['structs'][name]
		except KeyError:
			return None

	def loadfromfile(self, filename):
		'''
		Loads the spec from a file
		'''
		self.loadstack += 1
		with open(filename, 'r', encoding='utf-8') as fp:
			self.activecondition = ''
			self.currentfilename = filename
			self.linenumber = 1
			backslashed = ''
			while True:
				line = fp.readline()
				if not line:
					break

				# Remove leading and trailing whitespace
				line = line.strip()

				# If this line ends in a backslash, store it up for the next line instead
				if line.endswith('\\'):
					backslashed = line[:-1].strip()
				else:
					if backslashed:
						line = backslashed + ' ' + line
						backslashed = ''
					self.parseline(line)
				self.linenumber += 1
		self.loadstack -= 1

		if self.loadstack == 0:
			# If we're here, we're done reading the main file.
			if self.currentcommand:
				raise RuntimeError('Protocol spec ended in the middle of a command.')

			if self.currentprotocol:
				raise RuntimeError('Protocol spec ended in the middle of a protocol definition.')

	def parseline(self, line):
		'''
		Parses a single line of the spec.
		'''
		# Check for empty lines and comments
		if line == '' or line.startswith('#'):
			return

		# Check for a protocol definition
		match = re.search(r'^protocol\s+(\w+)$', line, re.IGNORECASE)
		if match:
			if self.currentprotocol:
				raise RuntimeError('Missing EndProtocol before new protocol definition')

			self.currentprotocol = {'name': match.group(1), 'commands': OrderedDict(), 'structs': OrderedDict()}
			self.protocols[match.group(1)] = self.currentprotocol
			return

		# Check for a command definition
		match = re.search(r'^command\s+(\w+)(?:\s+\:\s+(\w+))?$', line, re.IGNORECASE)
		if match:
			if self.currentcommand:
				raise RuntimeError('Missing EndCommand before new command definition')

			if not self.currentprotocol:
				raise RuntimeError('Missing Protocol before command definition')

			commandname = match.group(1)
			baseCommandName = match.group(2)

			if commandname in self.currentprotocol['commands']:
				raise RuntimeError('Tried to define command %s twice' % commandname)

			if baseCommandName:
				# Handle command inheritance
				if baseCommandName == commandname:
					raise RuntimeError('Cannot inherit command %s from itself' % commandname)
				if baseCommandName not in self.currentprotocol['commands']:
					raise RuntimeError('Cannot inherit %s from non-existent command %s' % (commandname, baseCommandName))

				# Deep-copy all attributes from the old command to this new one, so that the new command is independent.
				from copy import deepcopy
				baseCommand = self.currentprotocol['commands'][baseCommandName]
				self.currentcommand = deepcopy(baseCommand)
				self.currentcommand.name = commandname
				self.currentcommand.parent = baseCommand

				# Mark all members of this new command as inherited.
				for parameter in self.currentcommand:
					parameter.inherited = True
			else:
				self.currentcommand = SpecCommand(commandname)

			self.currentprotocol['commands'][commandname] = self.currentcommand
			return

		# Check for condition method name
		match = re.search(r'^CheckFunction\s+(\w+)$', line, re.IGNORECASE)
		if match:
			if not self.currentcommand or not self.activecondition:
				raise RuntimeError('CheckFunction may only be used inside an If block')
			self.currentcommand.conditionchecknames[self.activecondition] = match.group(1)
			return

		if line.lower() == 'unreliablecommand' and self.currentcommand:
			self.currentcommand.unreliable = True
			return

		if line.lower() == 'extendedcommand' and self.currentcommand:
			self.currentcommand.extended = True
			return

		# Check for struct definition. Note that this block has to come before the generic parameter one, or it will eat
		# structure definitions.
		match = re.search(r'^struct\s+([A-Za-z_]+)$', line, re.IGNORECASE)
		if match:
			if self.currentstruct:
				raise RuntimeError('Cannot nest a struct inside another struct')

			name = match.group(1)

			# Ensure that there's no built-in type by this name.
			if not catches(lambda: parametertypes.getParameterClass(name), AttributeError):
				raise RuntimeError('%s is a built-in type' % name)

			# Also ensure that there's no existing struct by this name.
			if self.findStructByName(name) is not None:
				raise RuntimeError('Struct %s already exists' % name)

			self.currentstruct = {'name': name, 'members': OrderedDict()}
			return

		# Parse command parameter, using another, more complicated, regular expression.
		#   attributes ────────────────────────────────────────────────────┐
		#   name ───────────────────────────────────────────┐              │
		#   array brackets ────────────────────────┐        │              │
		#   specialization ─────────────┐          │        │              │
		#   typename ────────┬───┐┌─────┴──────┐┌──┴──┐   ┌─┴─┐┌───────────┴───────────┐
		match = re.search(r'^(\w+)(?:<([^>]+)>)?(\[\])?\s+(\w+)(?:\s+with\s+([\w\s,]+))?$', line, re.IGNORECASE)
		if match:
			if not self.currentcommand and not self.currentstruct:
				raise RuntimeError('Tried to specify a parameter outside of a command or structure definition')

			typename, specialization, arraybrackets, name, attributes = match.groups()

			# Turn attributes into a set
			attributes = attributes and {x.lower().strip() for x in attributes.split(',')} or set()

			try:
				classtype = parametertypes.getParameterClass(typename)
			except AttributeError:
				raise RuntimeError('Unknown type: %s' % typename)

			if self.currentcommand:
				members = self.currentcommand.parameters
			else:
				members = self.currentstruct['members']

			if name in members.keys():
				raise RuntimeError('Tried to define %r twice in %s' % (name, self.currentcommand.name))

			# Build the parameter object and add it to the command.
			parm = classtype(typename = typename, name = name, specialization = specialization, attributes = attributes,
					condition = self.activecondition, spec = self)

			# If this is an array, wrap the parameter into an Array type.
			if arraybrackets:
				element = parm
				parm = parametertypes.ArrayParameter(typename = typename, name = name, condition = self.activecondition,
					spec = self, element = element)

			# Add this parameter to the members dictionary.
			members[name] = parm

			# If we have an active condition, add this parameter to the list of parameters affected by that
			# condition.
			if self.activecondition:
				self.currentcommand.conditions[self.activecondition].append(parm)
			return

		# Check for if clause
		match = re.search(r'^if\s*\((.+)\)$', line, re.IGNORECASE)
		if match:
			if self.activecondition:
				raise RuntimeError('If statements cannot be nested')
			self.activecondition = match.group(1)
			return

		if line.lower() == 'endcommand':
			if self.currentstruct:
				raise RuntimeError('Got EndCommand inside a struct definition')
			if not self.currentcommand:
				raise RuntimeError('Got EndCommand outside of a command definition')
			self.finishCurrentCommand()
			self.currentcommand = None
			return

		if line.lower() == 'endstruct':
			if not self.currentstruct:
				raise RuntimeError('Got EndStruct outside of a struct definition')
			self.currentprotocol['structs'][self.currentstruct['name']] = self.currentstruct
			self.currentstruct = None
			return

		if line.lower() == 'endif':
			if not self.activecondition:
				raise RuntimeError('Got EndIf without a preceding If')
			self.activecondition = ''
			return

		if line.lower() == 'endprotocol':
			if not self.currentprotocol:
				raise RuntimeError('Got EndProtocol outside of a protocol definition')
			if self.currentstruct:
				raise RuntimeError('Got EndProtocol inside a struct definition')
			if self.currentcommand:
				raise RuntimeError('Got EndProtocol inside a command definition')
			self.currentprotocol = None
			return

		# Check for include statements
		match = re.search(r'^include\s+"(.+)"$', line, re.IGNORECASE)
		if match:
			from os.path import join, dirname, realpath
			path = join(dirname(realpath(self.currentfilename)), match.group(1))
			oldfilename, oldlinenumber = self.currentfilename, self.linenumber
			self.loadfromfile(path)
			self.currentfilename, self.linenumber = oldfilename, oldlinenumber
			return

		# User clearly doesn't know how to write proper spec files. Shame on him!!
		raise RuntimeError('Bad syntax: ' + line)

	def finishCurrentCommand(self):
		if not self.currentcommand:
			return

		# Decide method names for each condition
		i = 1
		command = self.currentcommand

		for condition, parameters in command.conditions.items():
			if condition in command.conditionchecknames.keys():
				continue # We've already decided a name for it, don't override this

			from string import ascii_lowercase, digits
			if len(parameters) == 1:
				# There is only one parameter that is subject to this condition. So we can name the method after it.
				parm = parameters[0]
				name = 'Contains' + parm.name[0].upper() + parm.name[1:]
			elif (set(condition.lower()) - set(ascii_lowercase + digits)) == set():
				# The condition is an alphanumeric string, so use that.
				name = 'Check' + condition[0].upper() + condition[1:]
			else:
				# Otherwise, just make up something.
				name = 'CheckCondition%d' % i
				i += 1
			command.conditionchecknames[condition] = name

		# Fill in a dictionary mapping parameter names to method names
		command.parameterchecknames = {}
		for parameter in command:
			if parameter.condition:
				command.parameterchecknames[parameter.name] = command.conditionchecknames[parameter.condition]
			parameter.setter = 'Set' + parameter.name[0].upper() + parameter.name[1:]
		self.currentcommand = None

class SpecCommand:
	'''
	Represents a single server command in the spec file.
	'''
	def __init__(self, name, extended=False, parameters=None):
		self.name = name
		self.extended = extended
		self.parameters = parameters or OrderedDict()
		self.conditionnames = {}
		self.conditions = defaultdict(list)
		self.conditionchecknames = {} # condition string → method name
		self.unreliable = False
		self.parent = None

	@property
	def ownedParameters(self):
		''' Returns all parameters that were not inherited. '''
		for parameter in self.parameters.values():
			if parameter.inherited == False:
				yield parameter

	@property
	def enumname(self):
		''' Returns the SVC enum name of this command. '''
		return (self.extended and 'SVC2' or 'SVC') + '_' + self.name.upper()

	def __iter__(self):
		''' Provides support for iterating this command (yields the parameters) '''
		return iter(self.parameters.values())
