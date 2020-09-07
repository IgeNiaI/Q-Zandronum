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

from string import capwords

# These types are passed by value.
passbyvalue = {'int', 'unsigned int', 'short', 'unsigned short', 'long', 'unsigned long', 'bool', 'float', 'double',
			   'BYTE', 'SBYTE', 'WORD', 'SWORD', 'DWORD', 'SDWORD', 'QWORD', 'SQWORD',
			   'FName', 'fixed_t', 'angle_t', 'size_t'}

def getParameterClass(typename):
	'''
	Returns a class representing a the given parameter type
	'''
	from sys import modules
	classname = capwords(typename.lower()) + 'Parameter'
	return getattr(modules[__name__], classname)

def uppercasify(name):
	return name[:1].upper() + name[1:]

# ----------------------------------------------------------------------------------------------------------------------

class SpecParameter:
	'''
	Represents a parameter in a server command.
	'''
	def __init__(self, typename, name, specialization = None, attributes = None, condition = None, spec = None):
		self.typename = typename
		self.name = name
		self.specialization = specialization
		self.attributes = attributes or set()
		self.condition = condition and condition.strip()
		self.inherited = False # was this inherited from another command?

	def writeread(self, **args):
		raise Exception('BUG: %s does not define writeread!' % type(self).__name__)

	def writesend(self, **args):
		raise Exception('BUG: %s does not define writesend!' % type(self).__name__)

	def writereadchecks(self, **args):
		pass

	def writespecialmethods(self, **args):
		pass

	@property
	def constreference(self):
		if self.cxxtypename.endswith('*') or self.cxxtypename in passbyvalue:
			# Pass pointers and known types by value
			return self.cxxtypename
		else:
			# Otherwise use a const reference of own type name
			return 'const %s &' % self.cxxtypename

# ----------------------------------------------------------------------------------------------------------------------

class ByteParameter(SpecParameter):
	def __init__(self, **args):
		super().__init__(**args)

		if self.specialization:
			raise Exception('Integer types may not be specialized')

	# Writes code to read in this parameter.
	def writeread(self, writer, command, reference):
		readfunction = 'NETWORK_Read' + self.methodname()
		writer.writeline('command.{reference} = {readfunction}( bytestream );'.format(**locals()))

	# Writes code to write this parameter to a NetCommand.
	def writesend(self, writer, command, reference):
		sendmethod = 'command.add' + capwords(self.methodname())
		writer.writeline('{sendmethod}( this->{reference} );'.format(**locals()))

	# Returns the C++ type name for this parameter
	@property
	def cxxtypename(self):
		return 'int'

	def methodname(self):
		return 'Byte'

# ----------------------------------------------------------------------------------------------------------------------

class SbyteParameter(ByteParameter):
	@property
	def cxxtypename(self):
		return 'SBYTE'

# ----------------------------------------------------------------------------------------------------------------------

class ShortParameter(ByteParameter):
	def methodname(self):
		return 'Short'

# ----------------------------------------------------------------------------------------------------------------------

class UshortParameter(ShortParameter):
	@property
	def cxxtypename(self):
		return 'unsigned int'

# ----------------------------------------------------------------------------------------------------------------------

class LongParameter(ByteParameter):
	def methodname(self):
		return 'Long'

# ----------------------------------------------------------------------------------------------------------------------

class UlongParameter(LongParameter):
	@property
	def cxxtypename(self):
		return 'unsigned int'

# ----------------------------------------------------------------------------------------------------------------------

class StringParameter(SpecParameter):
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'FString'

	def writeread(self, writer, command, reference, **args):
		writer.writeline('command.{reference} = NETWORK_ReadString( bytestream );'.format(**locals()))

	def writesend(self, writer, command, reference, **args):
		writer.writeline('command.addString( this->{reference} );'.format(**locals()))

# ----------------------------------------------------------------------------------------------------------------------

class FloatParameter(SpecParameter):
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'float'

	def writeread(self, writer, command, reference, **args):
		writer.writeline('command.{reference} = NETWORK_ReadFloat( bytestream );'.format(**locals()))

	def writesend(self, writer, command, reference, **args):
		writer.writeline('command.addFloat( this->{reference} );'.format(**locals()))

# ----------------------------------------------------------------------------------------------------------------------

class BoolParameter(SpecParameter):
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'bool'

	def writeread(self, writer, command, reference, **args):
		writer.writeline('command.{reference} = NETWORK_ReadBit( bytestream );'.format(**locals()))

	def writesend(self, writer, command, reference, **args):
		writer.writecontext('command.addBit( this->{reference} );'.format(**locals()))

# ----------------------------------------------------------------------------------------------------------------------

class VariableParameter(SpecParameter):
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'int'

	def writeread(self, writer, command, reference, **args):
		writer.writeline('command.{reference} = NETWORK_ReadVariable( bytestream );'.format(**locals()))

	def writesend(self, writer, command, reference, **args):
		writer.writecontext('command.addVariable( this->{reference} );'.format(**locals()))

# ----------------------------------------------------------------------------------------------------------------------

class ShortbyteParameter(SpecParameter):
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'int'

	def writeread(self, writer, command, reference, **args):
		writer.writeline('command.{reference} = NETWORK_ReadShortByte( bytestream, {specialization} );'.format(specialization=self.specialization, **locals()))

	def writesend(self, writer, command, reference, **args):
		specialization = self.specialization
		writer.writecontext('command.addShortByte( this->{reference}, {specialization} );'.format(**locals()))

# ----------------------------------------------------------------------------------------------------------------------

class ActorParameter(SpecParameter):
	def __init__(self, **args):
		super().__init__(**args)

	@property
	def cxxtypename(self):
		if self.specialization:
			return self.specialization + ' *'
		else:
			return 'AActor *'

	def writeread(self, writer, command, reference, **args):
		# To read in an actor we'll first read in a short integer to represent the network id.
		# Then, we resolve this to the actor pointer.
		# If the actor pointer is specialized, we try to downcast it. If downcasting is not possible, the
		#     specialized pointer becomes NULL instead.
		# If the pointer is NULL and we haven't explicitly allowed nulls, the command is invalid and Execute()
		#     will not be called.
		netid = next(writer.tempvar)
		self.readnetid = netid # Remember the netid variable name for checks

		# Write the code to read in the netid
		writer.declare('int', netid)
		writer.writeline('{netid} = NETWORK_ReadShort( bytestream );'.format(**locals()))

	def writereadchecks(self, writer, command, reference, **args):
		netid = self.readnetid
		writer.writecontext('''
			if ( CLIENT_ReadActorFromNetID( {netid}, RUNTIME_CLASS( {specialization} ), {allownull},
											reinterpret_cast<AActor *&>( command.{reference} ),
											"{commandname}", "{reference}" ) == false )
			{{
				return true;
			}}

			'''.format(commandname=command.name, specialization=self.specialization or 'AActor',
					   allownull=('nullallowed' in self.attributes) and 'true' or 'false', **locals()))

	def writesend(self, writer, command, reference, **args):
		writer.writeline('command.addShort( this->{reference} ? this->{reference}->lNetID : -1 );'.format(**locals()))

# ----------------------------------------------------------------------------------------------------------------------

class ClassParameter(SpecParameter):
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'const PClass *'

	def writeread(self, writer, command, reference, **args):
		# To read in a class we'll first write code to read in the identification number, and then
		# use NETWORK_GetClassFromIdentification to turn it into the class parameter.
		netid = next(writer.tempvar)
		self.readnetid = netid
		writer.declare('int', netid)
		writer.writeline('{netid} = NETWORK_ReadShort( bytestream );'.format(**locals()))
		writer.writeline('command.{reference} = NETWORK_GetClassFromIdentification( {netid} );'.format(**locals()))

		# If the class parameter is specialized, ensure that it is a descendant of the specified class.
		# If not, NULL the parameter
		if self.specialization:
			writer.writeline('')
			writer.writecontext('''
				if ( command.{reference}->IsDescendantOf( RUNTIME_CLASS( {specialization} )) == false )
					command.{reference} = NULL;

				'''.format(reference=reference, specialization=self.specialization))

	def writereadchecks(self, writer, command, reference, **args):
		netid = self.readnetid

		# If we don't allow the parameter to be NULL, write code to check whether it is valid.
		if 'nullallowed' not in self.attributes:
			writer.writeline('')
			writer.writecontext('''
				if ( command.{reference} == NULL )
				{{
					CLIENT_PrintWarning( "{commandname}: unknown class ID for {reference}: %d\\n", {netid} );
					return true;
				}}

				'''.format(commandname = command.name, **locals()))

	def writesend(self, writer, command, reference, **args):
		writer.writeline('command.addShort( this->{reference} ? this->{reference}->getActorNetworkIndex() : -1 );'.format(**locals()))

# ----------------------------------------------------------------------------------------------------------------------

class PlayerParameter(SpecParameter):
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'player_t *'

	def writeread(self, writer, command, reference, **args):
		# Player self. We store both the index and a pointer to the player structure.
		writer.writeline('command.{reference} = &players[NETWORK_ReadByte( bytestream )];'.format(**locals()))

	def writereadchecks(self, writer, command, reference, **args):
		playernumber = next(writer.tempvar)
		writer.declare('int', playernumber)
		writer.writeline('{playernumber} = command.{reference} - players;'.format(**locals()))
		writer.writeline('')
		if 'indextestonly' in self.attributes:
			writer.writeline('if (( {playernumber} < 0 ) || ( {playernumber} >= MAXPLAYERS ))'.format(**locals()))
		else:
			writer.writeline('if ( PLAYER_IsValidPlayer( {playernumber} ) == false )'.format(**locals()))
		writer.writeline('{')
		writer.writeline('\tCLIENT_PrintWarning( "{commandname}: Invalid player number: %d\\n", {playernumber} );' \
			.format(commandname=command.name, **locals()))
		writer.writeline('\treturn true;')
		writer.writeline('}')
		writer.writeline('')

		# Allow for a check for the player's body
		if 'motest' in self.attributes:
			writer.writeline('')
			writer.writeline('if ( command.{reference}->mo == NULL )'.format(**locals()))
			writer.writeline('\treturn true;')
			writer.writeline('')

	def writesend(self, writer, command, reference, **args):
		writer.writeline('command.addByte( this->{reference} - players );'.format(**locals()))

# ----------------------------------------------------------------------------------------------------------------------

class Vector3Parameter(SpecParameter):
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'FVector3'

	def writeread(self, writer, command, reference, **args):
		writer.writecontext('''
			command.{reference}.X = NETWORK_ReadFloat( bytestream );
			command.{reference}.Y = NETWORK_ReadFloat( bytestream );
			command.{reference}.Z = NETWORK_ReadFloat( bytestream );'''.format(**locals()))

	def writesend(self, writer, command, reference, **args):
		writer.writecontext('''
			command.addFloat( this->{reference}.X );
			command.addFloat( this->{reference}.Y );
			command.addFloat( this->{reference}.Z );'''.format(**locals()))

# ----------------------------------------------------------------------------------------------------------------------

class FixedParameter(SpecParameter):
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'fixed_t'

	def writeread(self, writer, command, reference, **args):
		writer.writeline('command.{reference} = NETWORK_ReadLong( bytestream );'.format(**locals()))

	def writesend(self, writer, command, reference, **args):
		writer.writeline('command.addLong( this->{reference} );'.format(**locals()))

# ----------------------------------------------------------------------------------------------------------------------

class AproxfixedParameter(SpecParameter):
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'fixed_t'

	def writeread(self, writer, command, reference, **args):
		writer.writeline('command.{reference} = NETWORK_ReadShort( bytestream ) << FRACBITS;'.format(**locals()))

	def writesend(self, writer, command, reference, **args):
		writer.writeline('command.addShort( this->{reference} >> FRACBITS );'.format(**locals()))

# ----------------------------------------------------------------------------------------------------------------------

class AngleParameter(FixedParameter):
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'angle_t'

# ----------------------------------------------------------------------------------------------------------------------

class AproxangleParameter(AproxfixedParameter):
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'angle_t'

# ----------------------------------------------------------------------------------------------------------------------

class SectorParameter(SpecParameter):
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'sector_t *'
		self.resolvingFunction = 'CLIENT_FindSectorByID'
		self.arrayName = 'sectors'
		self.indexLength = 'Short'

	def writeread(self, writer, command, reference, **args):
		# We first store a temporary variable containing the sector number, and then use CLIENT_FindSectorByID to
		# get the sector parameter
		indexVariable = next(writer.tempvar)
		indexLength = self.indexLength
		resolvingFunction = self.resolvingFunction
		self.indexVariable = indexVariable
		writer.declare('int', indexVariable)
		writer.writeline('{indexVariable} = NETWORK_Read{indexLength}( bytestream );'.format(**locals()))
		writer.writeline('command.{reference} = {resolvingFunction}( {indexVariable} );'.format(**locals()))

	def writereadchecks(self, writer, command, reference, **args):
		indexVariable = self.indexVariable

		# If we don't allow the parameter to be NULL, write code to check whether it is valid.
		if 'nullallowed' not in self.attributes:
			writer.writeline('')
			writer.writecontext('''
				if ( command.{reference} == NULL )
				{{
					CLIENT_PrintWarning( "{commandname}: couldn't find {reference}: %d\\n", {indexVariable} );
					return true;
				}}

				'''.format(commandname = command.name, **locals()))

	def writesend(self, writer, command, reference, **args):
		arrayName = self.arrayName
		indexLength = self.indexLength
		writer.writeline('command.add{indexLength}( this->{reference} ? this->{reference} - {arrayName} : -1 );'.format(**locals()))

# ----------------------------------------------------------------------------------------------------------------------

class LineParameter(SectorParameter):
	# The line parameter acts exactly like the sector parameter, save for a few different identifiers.
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'line_t *'
		self.resolvingFunction = 'CLIENT_FindLineByID'
		self.arrayName = 'lines'
		self.indexLength = 'Short'

# ----------------------------------------------------------------------------------------------------------------------

class SideParameter(SectorParameter):
	# The side parameter is also similar.
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'side_t *'
		self.resolvingFunction = 'CLIENT_FindSideByID'
		self.arrayName = 'sides'
		self.indexLength = 'Long'

# ----------------------------------------------------------------------------------------------------------------------

class StructParameter(SpecParameter):
	'''
		The struct parameter handles custom compound types. Can be used with arrays and to avoid code duplication.
		The specification keeps track of struct registers. This class gets the struct definition from the spec,
		and implements its send, read and check writing by delegating them to its members.
	'''
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'struct %s' % args['specialization']
		self.struct = args['spec'].findStructByName(args['specialization'])

	def iterateMembers(self, reference):
		for member in self.struct['members'].values():
			yield member, reference + '.' + member.name

	def writeread(self, reference, **args):
		for member, membername in self.iterateMembers(reference):
			member.writeread(reference = membername, **args)

	def writesend(self, reference, **args):
		for member, membername in self.iterateMembers(reference):
			member.writesend(reference = membername, **args)

	def writereadchecks(self, reference, **args):
		for member, membername in self.iterateMembers(reference):
			member.writereadchecks(reference = membername, **args)

# ----------------------------------------------------------------------------------------------------------------------

class ArrayParameter(SpecParameter):
	def __init__(self, element, **args):
		super().__init__(**args)
		self.elementType = element
		self.cxxtypename = 'TArray<%s>' % self.elementType.cxxtypename

	def writeread(self, writer, reference, **args):
		# Use a size variable to store the size of this array.
		sizevariable = next(writer.tempvar)
		writer.declare('unsigned int', sizevariable)
		writer.writeline('%s = NETWORK_ReadByte( bytestream );' % sizevariable)

		# Allocate the array using the size we read.
		writer.writeline('command.%s.Reserve( %s );' % (reference, sizevariable))

		# Now iterate the array's elements, and write the reading code.
		writer.writeline('for ( unsigned int i = 0; i < %s; ++i )' % sizevariable)
		writer.startscope()
		self.elementType.writeread(writer = writer, reference = reference + '[i]', **args)
		writer.endscope()

	def writesend(self, writer, reference, **args):
		# If we're writing the sending code, write array's size into the command, and use it to loop over
		# the elements.
		writer.writeline('command.addByte( %s.Size() );' % reference)
		writer.writeline('for ( unsigned int i = 0; i < %s.Size(); ++i )' % reference)
		writer.startscope()
		self.elementType.writesend(writer = writer, reference = reference + '[i]', **args)
		writer.endscope()

	def writereadchecks(self, writer, reference, **args):
		# Iterate the array's elements and write read checks for all of them.
		writer.writeline('for ( unsigned int i = 0; i < command.%s.Size(); ++i )' % reference)
		writer.startscope()
		self.elementType.writereadchecks(writer = writer, reference = reference + '[i]', **args)
		writer.endscope()

	def writespecialmethods(self, writer, **args):
		# Add a method to push to this parameter.
		writer.writeline('void PushTo{name}({type} value)'.format(
			name = uppercasify(self.name),
			type = self.elementType.constreference,
		))
		writer.startscope()
		writer.writeline('{name}.Push(value);'.format(name = self.name))
		writer.writeline('_{name}Initialized = true;'.format(name = self.name))
		writer.endscope()
		# Add a method to pop from this parameter.
		writer.writeline('bool PopFrom{name}({type}& value)'.format(
			name = uppercasify(self.name),
			type = self.elementType.cxxtypename,
		))
		writer.startscope()
		writer.writeline('return {name}.Pop(value);'.format(name = self.name))
		writer.endscope()
		# Add a method to clear this parameter.
		writer.writeline('void Clear{name}()'.format(
			name = uppercasify(self.name),
		))
		writer.startscope()
		writer.writeline('{name}.Clear();'.format(name = self.name))
		writer.endscope()

# ----------------------------------------------------------------------------------------------------------------------

class NameParameter(SpecParameter):
	def __init__(self, **args):
		super().__init__(**args)
		self.cxxtypename = 'FName'

	def writeread(self, writer, command, reference, **args):
		writer.writeline('command.{reference} = NETWORK_ReadName( bytestream );'.format(**locals()))

	def writesend(self, writer, command, reference, **args):
		writer.writeline('command.addName( this->{reference} );'.format(**locals()))
