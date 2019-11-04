/* CTF format description.
   Copyright (C) 2019 Free Software Foundation, Inc.

   This file is part of libctf.

   libctf is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not see
   <http://www.gnu.org/licenses/>.  */

#ifndef	_CTF_H
#define	_CTF_H

#include <sys/types.h>
#include <limits.h>
#include <stdint.h>


#ifdef	__cplusplus
extern "C"
{
#endif

/* CTF - Compact ANSI-C Type Format

   This file format can be used to compactly represent the information needed
   by a debugger to interpret the ANSI-C types used by a given program.
   Traditionally, this kind of information is generated by the compiler when
   invoked with the -g flag and is stored in "stabs" strings or in the more
   modern DWARF format.  CTF provides a representation of only the information
   that is relevant to debugging a complex, optimized C program such as the
   operating system kernel in a form that is significantly more compact than
   the equivalent stabs or DWARF representation.  The format is data-model
   independent, so consumers do not need different code depending on whether
   they are 32-bit or 64-bit programs; libctf automatically compensates for
   endianness variations.  CTF assumes that a standard ELF symbol table is
   available for use in the debugger, and uses the structure and data of the
   symbol table to avoid storing redundant information.  The CTF data may be
   compressed on disk or in memory, indicated by a bit in the header.  CTF may
   be interpreted in a raw disk file, or it may be stored in an ELF section,
   typically named .ctf.  Data structures are aligned so that a raw CTF file or
   CTF ELF section may be manipulated using mmap(2).

   The CTF file or section itself has the following structure:

   +--------+--------+---------+----------+--------+----------+...
   |  file  |  type  |  data   | function | object | function |...
   | header | labels | objects |   info   | index  |  index   |...
   +--------+--------+---------+----------+--------+----------+...

   ...+----------+-------+--------+
   ...| variable | data  | string |
   ...|   info   | types | table  |
      +----------+-------+--------+

   The file header stores a magic number and version information, encoding
   flags, and the byte offset of each of the sections relative to the end of the
   header itself.  If the CTF data has been uniquified against another set of
   CTF data, a reference to that data also appears in the the header.  This
   reference is the name of the label corresponding to the types uniquified
   against.

   Following the header is a list of labels, used to group the types included in
   the data types section.  Each label is accompanied by a type ID i.  A given
   label refers to the group of types whose IDs are in the range [0, i].

   Data object and function records are stored in the same order as they appear
   in the corresponding symbol table, except that symbols marked SHN_UNDEF are
   not stored and symbols that have no type data are padded out with zeroes.
   For each data object, the type ID (a small integer) is recorded.  For each
   function, the type ID of the return type and argument types is recorded.

   For situations in which the order of the symbols in the symtab is not known,
   a pair of optional indexes follow the data object and function info sections:
   each of these is an array of strtab indexes, mapped 1:1 to the corresponding
   data object / function info section, giving each entry in those sections a
   name so that the linker can correlate them with final symtab entries and
   reorder them accordingly (dropping the indexes in the process).

   Variable records (as distinct from data objects) provide a modicum of support
   for non-ELF systems, mapping a variable name to a CTF type ID.  The variable
   names are sorted into ASCIIbetical order, permitting binary searching.  We do
   not define how the consumer maps these variable names to addresses or
   anything else, or indeed what these names represent: they might be names
   looked up at runtime via dlsym() or names extracted at runtime by a debugger
   or anything else the consumer likes.

   The data types section is a list of variable size records that represent each
   type, in order by their ID.  The types themselves form a directed graph,
   where each node may contain one or more outgoing edges to other type nodes,
   denoted by their ID.  Most type nodes are standalone or point backwards to
   earlier nodes, but this is not required: nodes can point to later nodes,
   particularly structure and union members.

   Strings are recorded as a string table ID (0 or 1) and a byte offset into the
   string table.  String table 0 is the internal CTF string table.  String table
   1 is the external string table, which is the string table associated with the
   ELF symbol table for this object.  CTF does not record any strings that are
   already in the symbol table, and the CTF string table does not contain any
   duplicated strings.

   If the CTF data has been merged with another parent CTF object, some outgoing
   edges may refer to type nodes that exist in another CTF object.  The debugger
   and libctf library are responsible for connecting the appropriate objects
   together so that the full set of types can be explored and manipulated.

   This connection is done purely using the ctf_import() function.  There is no
   notation anywhere in the child CTF file indicating which parent it is
   connected to: it is the debugger's responsibility to track this.  */

#define CTF_MAX_TYPE	0xfffffffe	/* Max type identifier value.  */
#define CTF_MAX_PTYPE	0x7fffffff	/* Max parent type identifier value.  */
#define CTF_MAX_NAME 0x7fffffff		/* Max offset into a string table.  */
#define CTF_MAX_VLEN	0xffffff /* Max struct, union, enum members or args.  */

/* See ctf_type_t */
#define CTF_MAX_SIZE	0xfffffffe	/* Max size of a v2 type in bytes. */
#define CTF_LSIZE_SENT	0xffffffff	/* Sentinel for v2 ctt_size.  */

# define CTF_MAX_TYPE_V1	0xffff	/* Max type identifier value.  */
# define CTF_MAX_PTYPE_V1	0x7fff	/* Max parent type identifier value.  */
# define CTF_MAX_VLEN_V1	0x3ff	/* Max struct, union, enums or args.  */
# define CTF_MAX_SIZE_V1	0xfffe	/* Max size of a type in bytes. */
# define CTF_LSIZE_SENT_V1	0xffff	/* Sentinel for v1 ctt_size.  */

  /* Start of actual data structure definitions.

     Every field in these structures must have corresponding code in the
     endianness-swapping machinery in libctf/ctf-open.c.  */

typedef struct ctf_preamble
{
  unsigned short ctp_magic;	/* Magic number (CTF_MAGIC).  */
  unsigned char ctp_version;	/* Data format version number (CTF_VERSION).  */
  unsigned char ctp_flags;	/* Flags (see below).  */
} ctf_preamble_t;

typedef struct ctf_header_v2
{
  ctf_preamble_t cth_preamble;
  uint32_t cth_parlabel;	/* Ref to name of parent lbl uniq'd against.  */
  uint32_t cth_parname;		/* Ref to basename of parent.  */
  uint32_t cth_lbloff;		/* Offset of label section.  */
  uint32_t cth_objtoff;		/* Offset of object section.  */
  uint32_t cth_funcoff;		/* Offset of function section.  */
  uint32_t cth_varoff;		/* Offset of variable section.  */
  uint32_t cth_typeoff;		/* Offset of type section.  */
  uint32_t cth_stroff;		/* Offset of string section.  */
  uint32_t cth_strlen;		/* Length of string section in bytes.  */
} ctf_header_v2_t;

typedef struct ctf_header
{
  ctf_preamble_t cth_preamble;
  uint32_t cth_parlabel;	/* Ref to name of parent lbl uniq'd against.  */
  uint32_t cth_parname;		/* Ref to basename of parent.  */
  uint32_t cth_cuname;		/* Ref to CU name (may be 0).  */
  uint32_t cth_lbloff;		/* Offset of label section.  */
  uint32_t cth_objtoff;		/* Offset of object section.  */
  uint32_t cth_funcoff;		/* Offset of function section.  */
  uint32_t cth_objtidxoff;	/* Offset of object index section.  */
  uint32_t cth_funcidxoff;	/* Offset of function index section.  */
  uint32_t cth_varoff;		/* Offset of variable section.  */
  uint32_t cth_typeoff;		/* Offset of type section.  */
  uint32_t cth_stroff;		/* Offset of string section.  */
  uint32_t cth_strlen;		/* Length of string section in bytes.  */
} ctf_header_t;

#define cth_magic   cth_preamble.ctp_magic
#define cth_version cth_preamble.ctp_version
#define cth_flags   cth_preamble.ctp_flags

#define CTF_MAGIC	0xdff2	/* Magic number identifying header.  */

/* Data format version number.  */

/* v1 upgraded to a later version is not quite the same as the native form,
   because the boundary between parent and child types is different but not
   recorded anywhere, and you can write it out again via ctf_compress_write(),
   so we must track whether the thing was originally v1 or not.  If we were
   writing the header from scratch, we would add a *pair* of version number
   fields to allow for this, but this will do for now.  (A flag will not do,
   because we need to encode both the version we came from and the version we
   went to, not just "we were upgraded".) */

# define CTF_VERSION_1 1
# define CTF_VERSION_1_UPGRADED_3 2
# define CTF_VERSION_2 3

#define CTF_VERSION_3 4
#define CTF_VERSION CTF_VERSION_3 /* Current version.  */

#define CTF_F_COMPRESS	0x1	/* Data buffer is compressed by libctf.  */

typedef struct ctf_lblent
{
  uint32_t ctl_label;		/* Ref to name of label.  */
  uint32_t ctl_type;		/* Last type associated with this label.  */
} ctf_lblent_t;

typedef struct ctf_varent
{
  uint32_t ctv_name;		/* Reference to name in string table.  */
  uint32_t ctv_type;		/* Index of type of this variable.  */
} ctf_varent_t;

/* In format v2, type sizes, measured in bytes, come in two flavours.  Nearly
   all of them fit into a (UINT_MAX - 1), and thus can be stored in the ctt_size
   member of a ctf_stype_t.  The maximum value for these sizes is CTF_MAX_SIZE.
   Types larger than this must be stored in the ctf_lsize member of a
   ctf_type_t.  Use of this member is indicated by the presence of
   CTF_LSIZE_SENT in ctt_size.  */

/* In v1, the same applies, only the limit is (USHRT_MAX - 1) and
   CTF_MAX_SIZE_V1, and CTF_LSIZE_SENT_V1 is the sentinel.  */

typedef struct ctf_stype_v1
{
  uint32_t ctt_name;		/* Reference to name in string table.  */
  unsigned short ctt_info;	/* Encoded kind, variant length (see below).  */
#ifndef __GNUC__
  union
  {
    unsigned short _size;	/* Size of entire type in bytes.  */
    unsigned short _type;	/* Reference to another type.  */
  } _u;
#else
  __extension__
  union
  {
    unsigned short ctt_size;	/* Size of entire type in bytes.  */
    unsigned short ctt_type;	/* Reference to another type.  */
  };
#endif
} ctf_stype_v1_t;

typedef struct ctf_type_v1
{
  uint32_t ctt_name;		/* Reference to name in string table.  */
  unsigned short ctt_info;	/* Encoded kind, variant length (see below).  */
#ifndef __GNUC__
  union
  {
    unsigned short _size;	/* Always CTF_LSIZE_SENT_V1.  */
    unsigned short _type;	/* Do not use.  */
  } _u;
#else
  __extension__
  union
  {
    unsigned short ctt_size;	/* Always CTF_LSIZE_SENT_V1.  */
    unsigned short ctt_type;	/* Do not use.  */
  };
#endif
  uint32_t ctt_lsizehi;		/* High 32 bits of type size in bytes.  */
  uint32_t ctt_lsizelo;		/* Low 32 bits of type size in bytes.  */
} ctf_type_v1_t;


typedef struct ctf_stype
{
  uint32_t ctt_name;		/* Reference to name in string table.  */
  uint32_t ctt_info;		/* Encoded kind, variant length (see below).  */
#ifndef __GNUC__
  union
  {
    uint32_t _size;		/* Size of entire type in bytes.  */
    uint32_t _type;		/* Reference to another type.  */
  } _u;
#else
  __extension__
  union
  {
    uint32_t ctt_size;		/* Size of entire type in bytes.  */
    uint32_t ctt_type;		/* Reference to another type.  */
  };
#endif
} ctf_stype_t;

typedef struct ctf_type
{
  uint32_t ctt_name;		/* Reference to name in string table.  */
  uint32_t ctt_info;		/* Encoded kind, variant length (see below).  */
#ifndef __GNUC__
union
  {
    uint32_t _size;		/* Always CTF_LSIZE_SENT.  */
    uint32_t _type;		/* Do not use.  */
  } _u;
#else
  __extension__
  union
  {
    uint32_t ctt_size;		/* Always CTF_LSIZE_SENT.  */
    uint32_t ctt_type;		/* Do not use.  */
  };
#endif
  uint32_t ctt_lsizehi;		/* High 32 bits of type size in bytes.  */
  uint32_t ctt_lsizelo;		/* Low 32 bits of type size in bytes.  */
} ctf_type_t;

#ifndef __GNUC__
#define ctt_size _u._size	/* For fundamental types that have a size.  */
#define ctt_type _u._type	/* For types that reference another type.  */
#endif

/* The following macros and inline functions compose and decompose values for
   ctt_info and ctt_name, as well as other structures that contain name
   references.  Use outside libdtrace-ctf itself is explicitly for access to CTF
   files directly: types returned from the library will always appear to be
   CTF_V2.

   v1: (transparently upgraded to v2 at open time: may be compiled out of the
   library)
               ------------------------
   ctt_info:   | kind | isroot | vlen |
               ------------------------
               15   11    10    9     0

   v2:
               ------------------------
   ctt_info:   | kind | isroot | vlen |
               ------------------------
               31    26    25  24     0

   CTF_V1 and V2 _INFO_VLEN have the same interface:

   kind = CTF_*_INFO_KIND(c.ctt_info);     <-- CTF_K_* value (see below)
   vlen = CTF_*_INFO_VLEN(fp, c.ctt_info); <-- length of variable data list

   stid = CTF_NAME_STID(c.ctt_name);     <-- string table id number (0 or 1)
   offset = CTF_NAME_OFFSET(c.ctt_name); <-- string table byte offset

   c.ctt_info = CTF_TYPE_INFO(kind, vlen);
   c.ctt_name = CTF_TYPE_NAME(stid, offset);  */

# define CTF_V1_INFO_KIND(info)		(((info) & 0xf800) >> 11)
# define CTF_V1_INFO_ISROOT(info)	(((info) & 0x0400) >> 10)
# define CTF_V1_INFO_VLEN(info)		(((info) & CTF_MAX_VLEN_V1))

#define CTF_V2_INFO_KIND(info)		(((info) & 0xfc000000) >> 26)
#define CTF_V2_INFO_ISROOT(info)	(((info) & 0x2000000) >> 25)
#define CTF_V2_INFO_VLEN(info)		(((info) & CTF_MAX_VLEN))

#define CTF_NAME_STID(name)		((name) >> 31)
#define CTF_NAME_OFFSET(name)		((name) & CTF_MAX_NAME)
#define CTF_SET_STID(name, stid)	((name) | (stid) << 31)

/* V2 only. */
#define CTF_TYPE_INFO(kind, isroot, vlen) \
	(((kind) << 26) | (((isroot) ? 1 : 0) << 25) | ((vlen) & CTF_MAX_VLEN))

#define CTF_TYPE_NAME(stid, offset) \
	(((stid) << 31) | ((offset) & CTF_MAX_NAME))

/* The next set of macros are for public consumption only.  Not used internally,
   since the relevant type boundary is dependent upon the version of the file at
   *opening* time, not the version after transparent upgrade.  Use
   ctf_type_isparent() / ctf_type_ischild() for that.  */

#define CTF_V2_TYPE_ISPARENT(fp, id)	((id) <= CTF_MAX_PTYPE)
#define CTF_V2_TYPE_ISCHILD(fp, id)	((id) > CTF_MAX_PTYPE)
#define CTF_V2_TYPE_TO_INDEX(id)	((id) & CTF_MAX_PTYPE)
#define CTF_V2_INDEX_TO_TYPE(id, child) ((child) ? ((id) | (CTF_MAX_PTYPE+1)) : (id))

# define CTF_V1_TYPE_ISPARENT(fp, id)	((id) <= CTF_MAX_PTYPE_V1)
# define CTF_V1_TYPE_ISCHILD(fp, id)	((id) > CTF_MAX_PTYPE_V1)
# define CTF_V1_TYPE_TO_INDEX(id)	((id) & CTF_MAX_PTYPE_V1)
# define CTF_V1_INDEX_TO_TYPE(id, child) ((child) ? ((id) | (CTF_MAX_PTYPE_V1+1)) : (id))

/* Valid for both V1 and V2. */
#define CTF_TYPE_LSIZE(cttp) \
	(((uint64_t)(cttp)->ctt_lsizehi) << 32 | (cttp)->ctt_lsizelo)
#define CTF_SIZE_TO_LSIZE_HI(size)	((uint32_t)((uint64_t)(size) >> 32))
#define CTF_SIZE_TO_LSIZE_LO(size)	((uint32_t)(size))

#define CTF_STRTAB_0	0	/* String table id 0 (in-CTF).  */
#define CTF_STRTAB_1	1	/* String table id 1 (ELF strtab).  */

/* Values for CTF_TYPE_KIND().  If the kind has an associated data list,
   CTF_INFO_VLEN() will extract the number of elements in the list, and
   the type of each element is shown in the comments below. */

#define CTF_K_UNKNOWN	0	/* Unknown type (used for padding).  */
#define CTF_K_INTEGER	1	/* Variant data is CTF_INT_DATA (see below).  */
#define CTF_K_FLOAT	2	/* Variant data is CTF_FP_DATA (see below).  */
#define CTF_K_POINTER	3	/* ctt_type is referenced type.  */
#define CTF_K_ARRAY	4	/* Variant data is single ctf_array_t.  */
#define CTF_K_FUNCTION	5	/* ctt_type is return type, variant data is
				   list of argument types (unsigned short's for v1,
				   uint32_t's for v2).  */
#define CTF_K_STRUCT	6	/* Variant data is list of ctf_member_t's.  */
#define CTF_K_UNION	7	/* Variant data is list of ctf_member_t's.  */
#define CTF_K_ENUM	8	/* Variant data is list of ctf_enum_t's.  */
#define CTF_K_FORWARD	9	/* No additional data; ctt_name is tag.  */
#define CTF_K_TYPEDEF	10	/* ctt_type is referenced type.  */
#define CTF_K_VOLATILE	11	/* ctt_type is base type.  */
#define CTF_K_CONST	12	/* ctt_type is base type.  */
#define CTF_K_RESTRICT	13	/* ctt_type is base type.  */
#define CTF_K_SLICE	14	/* Variant data is a ctf_slice_t.  */

#define CTF_K_MAX	63	/* Maximum possible (V2) CTF_K_* value.  */

/* Values for ctt_type when kind is CTF_K_INTEGER.  The flags, offset in bits,
   and size in bits are encoded as a single word using the following macros.
   (However, you can also encode the offset and bitness in a slice.)  */

#define CTF_INT_ENCODING(data) (((data) & 0xff000000) >> 24)
#define CTF_INT_OFFSET(data)   (((data) & 0x00ff0000) >> 16)
#define CTF_INT_BITS(data)     (((data) & 0x0000ffff))

#define CTF_INT_DATA(encoding, offset, bits) \
       (((encoding) << 24) | ((offset) << 16) | (bits))

#define CTF_INT_SIGNED	0x01	/* Integer is signed (otherwise unsigned).  */
#define CTF_INT_CHAR	0x02	/* Character display format.  */
#define CTF_INT_BOOL	0x04	/* Boolean display format.  */
#define CTF_INT_VARARGS	0x08	/* Varargs display format.  */

/* Use CTF_CHAR to produce a char that agrees with the system's native
   char signedness.  */
#if CHAR_MIN == 0
# define CTF_CHAR (CTF_INT_CHAR)
#else
# define CTF_CHAR (CTF_INT_CHAR | CTF_INT_SIGNED)
#endif

/* Values for ctt_type when kind is CTF_K_FLOAT.  The encoding, offset in bits,
   and size in bits are encoded as a single word using the following macros.
   (However, you can also encode the offset and bitness in a slice.)  */

#define CTF_FP_ENCODING(data)  (((data) & 0xff000000) >> 24)
#define CTF_FP_OFFSET(data)    (((data) & 0x00ff0000) >> 16)
#define CTF_FP_BITS(data)      (((data) & 0x0000ffff))

#define CTF_FP_DATA(encoding, offset, bits) \
       (((encoding) << 24) | ((offset) << 16) | (bits))

/* Variant data when kind is CTF_K_FLOAT is an encoding in the top eight bits.  */
#define CTF_FP_ENCODING(data)	(((data) & 0xff000000) >> 24)

#define CTF_FP_SINGLE	1	/* IEEE 32-bit float encoding.  */
#define CTF_FP_DOUBLE	2	/* IEEE 64-bit float encoding.  */
#define CTF_FP_CPLX	3	/* Complex encoding.  */
#define CTF_FP_DCPLX	4	/* Double complex encoding.  */
#define CTF_FP_LDCPLX	5	/* Long double complex encoding.  */
#define CTF_FP_LDOUBLE	6	/* Long double encoding.  */
#define CTF_FP_INTRVL	7	/* Interval (2x32-bit) encoding.  */
#define CTF_FP_DINTRVL	8	/* Double interval (2x64-bit) encoding.  */
#define CTF_FP_LDINTRVL	9	/* Long double interval (2x128-bit) encoding.  */
#define CTF_FP_IMAGRY	10	/* Imaginary (32-bit) encoding.  */
#define CTF_FP_DIMAGRY	11	/* Long imaginary (64-bit) encoding.  */
#define CTF_FP_LDIMAGRY	12	/* Long double imaginary (128-bit) encoding.  */

#define CTF_FP_MAX	12	/* Maximum possible CTF_FP_* value */

/* A slice increases the offset and reduces the bitness of the referenced
   ctt_type, which must be a type which has an encoding (fp, int, or enum).  We
   also store the referenced type in here, because it is easier to keep the
   ctt_size correct for the slice than to shuffle the size into here and keep
   the ctt_type where it is for other types.

   In a future version, where we loosen requirements on alignment in the CTF
   file, the cts_offset and cts_bits will be chars: but for now they must be
   shorts or everything after a slice will become unaligned.  */

typedef struct ctf_slice
{
  uint32_t cts_type;
  unsigned short cts_offset;
  unsigned short cts_bits;
} ctf_slice_t;

typedef struct ctf_array_v1
{
  unsigned short cta_contents;	/* Reference to type of array contents.  */
  unsigned short cta_index;	/* Reference to type of array index.  */
  uint32_t cta_nelems;		/* Number of elements.  */
} ctf_array_v1_t;

typedef struct ctf_array
{
  uint32_t cta_contents;	/* Reference to type of array contents.  */
  uint32_t cta_index;		/* Reference to type of array index.  */
  uint32_t cta_nelems;		/* Number of elements.  */
} ctf_array_t;

/* Most structure members have bit offsets that can be expressed using a short.
   Some don't.  ctf_member_t is used for structs which cannot contain any of
   these large offsets, whereas ctf_lmember_t is used in the latter case.  If
   any member of a given struct has an offset that cannot be expressed using a
   uint32_t, all members will be stored as type ctf_lmember_t.  This is expected
   to be very rare (but nonetheless possible).  */

#define CTF_LSTRUCT_THRESH	536870912

/* In v1, the same is true, except that lmembers are used for structs >= 8192
   bytes in size.  (The ordering of members in the ctf_member_* structures is
   different to improve padding.)  */

#define CTF_LSTRUCT_THRESH_V1	8192

typedef struct ctf_member_v1
{
  uint32_t ctm_name;		/* Reference to name in string table.  */
  unsigned short ctm_type;	/* Reference to type of member.  */
  unsigned short ctm_offset;	/* Offset of this member in bits.  */
} ctf_member_v1_t;

typedef struct ctf_lmember_v1
{
  uint32_t ctlm_name;		/* Reference to name in string table.  */
  unsigned short ctlm_type;	/* Reference to type of member.  */
  unsigned short ctlm_pad;	/* Padding.  */
  uint32_t ctlm_offsethi;	/* High 32 bits of member offset in bits.  */
  uint32_t ctlm_offsetlo;	/* Low 32 bits of member offset in bits.  */
} ctf_lmember_v1_t;

typedef struct ctf_member_v2
{
  uint32_t ctm_name;		/* Reference to name in string table.  */
  uint32_t ctm_offset;		/* Offset of this member in bits.  */
  uint32_t ctm_type;		/* Reference to type of member.  */
} ctf_member_t;

typedef struct ctf_lmember_v2
{
  uint32_t ctlm_name;		/* Reference to name in string table.  */
  uint32_t ctlm_offsethi;	/* High 32 bits of member offset in bits.  */
  uint32_t ctlm_type;		/* Reference to type of member.  */
  uint32_t ctlm_offsetlo;	/* Low 32 bits of member offset in bits.  */
} ctf_lmember_t;

#define	CTF_LMEM_OFFSET(ctlmp) \
	(((uint64_t)(ctlmp)->ctlm_offsethi) << 32 | (ctlmp)->ctlm_offsetlo)
#define	CTF_OFFSET_TO_LMEMHI(offset)	((uint32_t)((uint64_t)(offset) >> 32))
#define	CTF_OFFSET_TO_LMEMLO(offset)	((uint32_t)(offset))

typedef struct ctf_enum
{
  uint32_t cte_name;		/* Reference to name in string table.  */
  int32_t cte_value;		/* Value associated with this name.  */
} ctf_enum_t;

/* The ctf_archive is a collection of ctf_file_t's stored together. The format
   is suitable for mmap()ing: this control structure merely describes the
   mmap()ed archive (and overlaps the first few bytes of it), hence the
   greater care taken with integral types.  All CTF files in an archive
   must have the same data model.  (This is not validated.)

   All integers in this structure are stored in little-endian byte order.

   The code relies on the fact that everything in this header is a uint64_t
   and thus the header needs no padding (in particular, that no padding is
   needed between ctfa_ctfs and the unnamed ctfa_archive_modent array
   that follows it).

   This is *not* the same as the data structure returned by the ctf_arc_*()
   functions:  this is the low-level on-disk representation.  */

#define CTFA_MAGIC 0x8b47f2a4d7623eeb	/* Random.  */
struct ctf_archive
{
  /* Magic number.  (In loaded files, overwritten with the file size
     so ctf_arc_close() knows how much to munmap()).  */
  uint64_t ctfa_magic;

  /* CTF data model.  */
  uint64_t ctfa_model;

  /* Number of CTF files in the archive.  */
  uint64_t ctfa_nfiles;

  /* Offset of the name table.  */
  uint64_t ctfa_names;

  /* Offset of the CTF table.  Each element starts with a size (a uint64_t
     in network byte order) then a ctf_file_t of that size.  */
  uint64_t ctfa_ctfs;
};

/* An array of ctfa_nnamed of this structure lies at
   ctf_archive[ctf_archive->ctfa_modents] and gives the ctfa_ctfs or
   ctfa_names-relative offsets of each name or ctf_file_t.  */

typedef struct ctf_archive_modent
{
  uint64_t name_offset;
  uint64_t ctf_offset;
} ctf_archive_modent_t;

#ifdef	__cplusplus
}
#endif

#endif				/* _CTF_H */
