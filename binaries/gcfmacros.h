// gcfmacros.h - Miscellaneous macros
//
// Use of this source code is governed by the GPLv2 license that can be found
//   in the LICENSE file.
//
// Version 0.1
// Author: Saranga Komanduri
//   Based on code originally written and published by Matt Weir under the
//   GPLv2 license.
//
// Modified: Wed May 28 17:07:11 2014
//

#ifndef GCFMACROS_H__
#define GCFMACROS_H__

// From http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml#Declaration_Order
//   Disable copy and assign operations unless code is explicitly written to support them
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

#endif // GCFMACROS_H__
