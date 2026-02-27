#ifdef INCLUDE_RCSID_C
const char rcsid_kegswin_dirent_h[] = "@(#)$KmKId: win_dirent.h,v 1.2 2022-02-11 04:13:45+00 kentd Exp $";
#endif

/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2022 by Kent Dickey			*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

// Hacky defines to get something to compile for now
typedef unsigned short mode_t;

struct dirent {
	char	d_name[1024];
};

struct DIR_t {
	int	find_data_valid;
	void	*win_handle;
	void	*find_data_ptr;
	struct dirent dirent;
};
typedef struct DIR_t DIR;

DIR *opendir(const char *filename);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);

