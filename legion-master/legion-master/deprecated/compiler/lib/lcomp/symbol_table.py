#!/usr/bin/env python

# Copyright 2014 Stanford University and Los Alamos National Security, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

###
### Symbol Table
###

class UndefinedSymbolException(Exception): pass
class RedefinedSymbolException(Exception): pass

class SymbolTable:
    def __init__(self, parent = None):
        self.parent = parent
        self.scope = {}
    def new_scope(self):
        return SymbolTable(self)
    def insert(self, key, val, shadow = False):
        if not shadow and key in self.scope:
            raise RedefinedSymbolException(
                'Name %s is already defined' % (key,))
        self.scope[key] = val
    def lookup(self, key):
        if key in self.scope:
            return self.scope[key]
        if self.parent is not None:
            return self.parent.lookup(key)
        raise UndefinedSymbolException(
            'Name %s is undefined' % (key,))
