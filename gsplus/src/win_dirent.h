#ifdef INCLUDE_RCSID_C
#endif

/**********************************************************************/
/*                    GSplus - Apple //gs Emulator                    */
/*                    Based on KEGS by Kent Dickey                    */
/*                    Copyright 2022 Kent Dickey                      */
/*                    Copyright 2025-2026 GSplus Contributors         */
/*                                                                    */
/*      This code is covered by the GNU GPL v3                        */
/*      See the file COPYING.txt or https://www.gnu.org/licenses/     */
/**********************************************************************/

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

