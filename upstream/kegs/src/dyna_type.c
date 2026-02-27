const char rcsid_dynatype_c[] = "@(#)$KmKId: dyna_type.c,v 1.9 2023-05-19 13:52:30+00 kentd Exp $";

/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2021-2023 by Kent Dickey		*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

#include "defc.h"

// Provide routines for Dynapro to use for detecting the file type on the
//  host system.  Host files can be "basic1.bas", "basic2,tbas,a$801"

STRUCT(Dynatype_extensions) {
	char	str[16];
	word16	file_type;
	word16	aux_type;
};

Dynatype_extensions g_dynatype_extensions[] = {
{	"applesingle",		0xfff,	0xffff },
{	"txt",			0x04,	0 },
{	"c",			0x04,	0 },			// ,ttxt
{	"s",			0x04,	0 },			// ,ttxt
{	"h",			0x04,	0 },			// ,ttxt
{	"bin",			0x06,	0x2000 },		// ,tbin
{	"bas",			0xfc,	0x0801 },		// ,tbas
{	"system",		0xff,	0x2000 },		// ,tsys
//{	"shr",			0xc0,	0x0002 },		// ,t$c0
{	"shk",			0xe0,	0x8002 },		// ,t$e0
{	"sdk",			0xe0,	0x8002 },		// ,t$e0
{	"",			0,	0 }
};

STRUCT(Dynatype_types) {
	char	str[16];
	word16	file_type;
	word16	aux_type;
};

Dynatype_types g_dynatype_types[] = {
{	"non",		0x00,	0 },
{	"bad",		0x01,	0 },
{	"txt",		0x04,	0 },
{	"bin",		0x06,	0x2000 },
{	"pnt",		0xc0,	0x0002 },
{	"fnd",		0xc9,	0 },
{	"icn",		0xca,	0 },
{	"cmd",		0xf0,	0 },
{	"bas",		0xfc,	0x0801 },
{	"sys",		0xff,	0x2000 },
{	"",		0,	0 }
};

word32
dynatype_scan_extensions(const char *str)
{
	int	len;
	int	i;

	len = (int)(sizeof(g_dynatype_extensions) /
					sizeof(g_dynatype_extensions[0]));
	for(i = 0; i < len; i++) {
		if(cfgcasecmp(str, g_dynatype_extensions[i].str) == 0) {
			return (g_dynatype_extensions[i].file_type << 16) |
					g_dynatype_extensions[i].aux_type |
					0x1000000;
		}
	}

	return 0;
}

word32
dynatype_find_prodos_type(const char *str)
{
	word32	file_type;
	int	len;
	int	i;

	len = (int)(sizeof(g_dynatype_types) / sizeof(g_dynatype_types[0]));
	for(i = 0; i < len; i++) {
		if(cfgcasecmp(str, g_dynatype_types[i].str) == 0) {
			file_type = g_dynatype_types[i].file_type;
			return (file_type << 16) | g_dynatype_types[i].aux_type;
		}
	}

	return 0;
}

const char *
dynatype_find_file_type(word32 file_type)
{
	int	len;
	int	i;

	len = (int)(sizeof(g_dynatype_types) / sizeof(g_dynatype_types[0]));
	for(i = 0; i < len; i++) {
		if(g_dynatype_types[i].file_type == file_type) {
			return g_dynatype_types[i].str;
		}
	}

	return 0;
}

word32
dynatype_detect_file_type(Dynapro_file *fileptr, const char *path_ptr,
							word32 storage_type)
{
	char	ext_buf[32];
	const char *str;
	char	*endstr;
	word32	file_type, aux_type, type_or_aux;
	int	len, this_len, c, pos;

	// Look for ,tbas and ,a$2000 to get filetype and aux_type info

	str = cfg_str_basename(path_ptr);
	len = (int)strlen(str);

	// Look for .ext and ,tbas, etc.
	pos = 0;
	ext_buf[0] = 0;
	file_type = 0x06;			// Default to BIN
	aux_type = 0;
	while(pos < len) {
		c = str[pos++];
		if(c == '.') {
			this_len = dynatype_get_extension(&str[pos],
							&ext_buf[0], 30);
			pos += this_len;
			continue;
		} else if(c == ',') {
			this_len = dynatype_comma_arg(&str[pos], &type_or_aux);
			if(type_or_aux & 0x1000000) {
				file_type = type_or_aux;
			} else if(type_or_aux & 0x2000000) {
				aux_type = type_or_aux;
			} else {
				printf("Unknown , extension, %s ignored\n",
								&str[pos]);
			}
			pos += this_len;
			continue;
		} else if(c == '#') {
			// Cadius style encoding: #ff2000 is type=$ff, aux=$2000
			type_or_aux = strtol(&str[pos], &endstr, 16);
			file_type = (type_or_aux & 0xffffff) | 0x1000000;
			aux_type = 0;
			pos += (int)(endstr - str);
			continue;
		}
	}

	// Handle extensions and type.  First do extension mapping
	if(ext_buf[0]) {
		type_or_aux = dynatype_scan_extensions(&ext_buf[0]);
		if((type_or_aux) >= 0x0f000000UL) {
			// AppleSingle
			storage_type = 0x50;		// Forked file
		}
		if(file_type < 0x1000000) {
			file_type = type_or_aux;
		}
		if(aux_type < 0x1000000) {
			aux_type = type_or_aux;
		}
	}
#if 0
	printf("After parsing ext, file_type:%08x, aux_type:%08x\n",
						file_type, aux_type);
#endif

	fileptr->file_type = (file_type >> 16) & 0xff;
	if(aux_type == 0) {
		aux_type = file_type & 0xffff;
	}
	fileptr->aux_type = aux_type & 0xffff;

	return storage_type;
}

int
dynatype_get_extension(const char *str, char *out_ptr, int buf_len)
{
	int	c, len;

	// Will write up to buf_len chars to out_ptr
	if(buf_len < 1) {
		return 0;
	}
	buf_len--;
	len = 0;
	while(1) {
		c = *str++;
		*out_ptr = c;
		if((c == 0) || (c == '.') || (c == ',') || (c == '#') ||
							(len >= buf_len)) {
			*out_ptr = 0;
			return len;
		}
		out_ptr++;
		len++;
	}
}

int
dynatype_comma_arg(const char *str, word32 *type_or_aux_ptr)
{
	char	type_buf[8];
	char	*endstr;
	word32	val, type_or_aux;
	int	c, len, base, this_len;
	int	i;

	// Read next char
	*type_or_aux_ptr = 0;

	c = *str++;
	if(c == 0) {
		return 0;
	}
	len = 1;
	c = tolower(c);
	type_or_aux = c;
	// See if next char is $ for hex
	c = *str;
	base = 0;
	if(c == '$') {
		base = 16;
		str++;
		len++;
	}
	val = strtol(str, &endstr, base);
	this_len = (int)(endstr - str);
	if((val == 0) && (this_len < 2) && (base == 0) &&
							(type_or_aux == 't')) {
		// Not a valid number
		for(i = 0; i < 3; i++) {
			c = *str++;
			if(c == 0) {
				return len;
			}
			type_buf[i] = c;
			len++;
		}
		type_buf[3] = 0;
		val = dynatype_find_prodos_type(&type_buf[0]);
		*type_or_aux_ptr = 0x1000000 | val;
	} else {
		len += this_len;
	}
	if(type_or_aux == 't') {
		if(val < 0x100) {
			*type_or_aux_ptr = 0x1000000 | ((val << 16) & 0xffffff);
		}
	} else if(type_or_aux == 'a') {
		*type_or_aux_ptr = 0x2000000 | (val & 0xffff);
	}

	return len;
}

void
dynatype_fix_unix_name(Dynapro_file *fileptr, char *outbuf_ptr, int path_max)
{
	char	buf[16];
	Dynapro_file tmpfile;
	const char *str;
	word32	aux_type;

#if 0
	printf("Looking at %s ftype:%02x aux:%04x\n", outbuf_ptr,
					fileptr->file_type, fileptr->aux_type);
#endif

	if(fileptr->prodos_name[0] >= 0xd0) {
		return;			// Directory, or Dir/Volume Header
	}
	if((fileptr->prodos_name[0] & 0xf0) == 0x50) {
		// Forked file, add .applesingle
		cfg_strlcat(outbuf_ptr, ".applesingle", path_max);
		return;
	}

	memset(&tmpfile, 0, sizeof(Dynapro_file));

	// See what this file defaults to as to type/aux
	(void)dynatype_detect_file_type(&tmpfile, outbuf_ptr, 0x10);

	// Otherwise, add ,ttype and ,a$aux as needed
	if(tmpfile.file_type != fileptr->file_type) {
		str = dynatype_find_file_type(fileptr->file_type);
		if(str) {
			aux_type = dynatype_find_prodos_type(str);
		} else {
			str = &buf[0];
			buf[15] = 0;
			snprintf(&buf[0], 15, "$%02x", fileptr->file_type);
		}
		cfg_strlcat(outbuf_ptr, ",t", path_max);
		cfg_strlcat(outbuf_ptr, str, path_max);
	}

	(void)dynatype_detect_file_type(&tmpfile, outbuf_ptr, 0x10);
	aux_type = fileptr->aux_type;

	if(aux_type != tmpfile.aux_type) {
		buf[15] = 0;
		snprintf(&buf[0], 15, ",a$%04x", aux_type & 0xffff);
		cfg_strlcat(outbuf_ptr, &buf[0], path_max);
	}

	// printf("dynatype_new_unix_name: %s\n", outbuf_ptr);

	// Check that it succeeded
	(void)dynatype_detect_file_type(&tmpfile, outbuf_ptr, 0x10);
	if((tmpfile.file_type != fileptr->file_type) ||
				(tmpfile.aux_type != fileptr->aux_type)) {
		halt_printf("File %s want ftype:%02x aux:%04x, got:%02x %04x\n",
			outbuf_ptr, fileptr->file_type, fileptr->aux_type,
			tmpfile.file_type, tmpfile.aux_type);
		exit(1);
	}
}
