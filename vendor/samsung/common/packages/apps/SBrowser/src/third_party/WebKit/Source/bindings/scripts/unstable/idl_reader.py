# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Read an IDL file or complete IDL interface, producing an IdlDefinitions object."""

import os.path

import blink_idl_parser
import idl_definitions_builder
import idl_validator
import interface_dependency_resolver


class IdlReader:
    def __init__(self, interfaces_info=None, idl_attributes_filename=None, outputdir=''):
        if idl_attributes_filename:
            self.extended_attribute_validator = idl_validator.IDLExtendedAttributeValidator(idl_attributes_filename)
        else:
            self.extended_attribute_validator = None

        if interfaces_info:
            self.interface_dependency_resolver = interface_dependency_resolver.InterfaceDependencyResolver(interfaces_info, self)
        else:
            self.interface_dependency_resolver = None

        self.parser = blink_idl_parser.BlinkIDLParser(outputdir=outputdir)

    def read_idl_definitions(self, idl_filename):
        """Returns an IdlDefinitions object for an IDL file, including all dependencies."""
        definitions = self.read_idl_file(idl_filename)
        if not self.interface_dependency_resolver:
            return definitions

        interface_name, _ = os.path.splitext(os.path.basename(idl_filename))
        self.interface_dependency_resolver.resolve_dependencies(
            definitions, interface_name)
        return definitions

    def read_idl_file(self, idl_filename):
        """Returns an IdlDefinitions object for an IDL file, without any dependencies."""
        ast = blink_idl_parser.parse_file(self.parser, idl_filename)
        definitions = idl_definitions_builder.build_idl_definitions_from_ast(ast)
        if not self.extended_attribute_validator:
            return definitions

        try:
            self.extended_attribute_validator.validate_extended_attributes(definitions)
        except idl_validator.IDLInvalidExtendedAttributeError as error:
            raise idl_validator.IDLInvalidExtendedAttributeError("""IDL ATTRIBUTE ERROR in file %s:
    %s
If you want to add a new IDL extended attribute, please add it to
    bindings/IDLExtendedAttributes.txt
and add an explanation to the Blink IDL documentation at:
    http://www.chromium.org/blink/webidl/blink-idl-extended-attributes
    """ % (idl_filename, str(error)))

        return definitions
