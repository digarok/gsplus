#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


#define HISTORY_SIZE 64

/* remove trailing whitespace and terminators */
static void cleanup_buffer(char *buffer, int size) {

	while (size > 0) {
		unsigned c = buffer[size-1];
		if (c == ' ' || c == '\r' || c == '\n') {
			--size;
			continue;
		}
		break;
	}
	buffer[size] = 0;
}

#if defined(USE_LIBEDIT)

#include <histedit.h>


	/* BSD libedit support */
EditLine *el = NULL;
History *hist = NULL;
HistEvent ev;

static const char *el_prompt = NULL;
static char *prompt_fn(EditLine *el) {
	return el_prompt ? (char *)el_prompt : "";
}

char *x_readline(const char *prompt) {
	static char buffer[1024];
	int count = 0;
	const char *cp;

	if (!el) {
		hist = history_init();
		history(hist, &ev, H_SETSIZE, HISTORY_SIZE);
		history(hist, &ev, H_SETUNIQUE, 1);

		el = el_init("GS+", stdin, stdout, stderr);
		el_set(el, EL_EDITOR, "emacs");
		el_set(el, EL_BIND, "-e", NULL, NULL, NULL);
		el_set(el, EL_HIST, history, hist);
		el_set(el, EL_PROMPT, prompt_fn);
		el_set(el, EL_SIGNAL, 1);
		el_source(el, NULL);
	}

	el_prompt = prompt;
	cp = el_gets(el, &count);
	el_prompt = NULL;
	if (count <= 0) return NULL;
	if (count > sizeof(buffer) - 1) return "";


	memcpy(buffer, cp, count);
	cleanup_buffer(buffer, count);

	if (*buffer)
		history(hist, &ev, H_ENTER, buffer);

	return buffer;
}

void x_readline_end(void) {
	if (el) {
		el_end(el);
		el = NULL;
	}
	if (hist) {
		history_end(hist);
		hist = NULL;
	}
}

#elif defined(USE_READLINE)
	/* GNU Readline support */

#include <readline/readline.h>
#include <readline/history.h>

static int readline_init = 0;
/* suppress tab completion, which defaults to filenames */
static char **rl_acf(const char* text, int start, int end) {
	return NULL;
}
char *x_readline(const char *prompt) {
	static char buffer[1024];

	char *cp;
	int count;

	if (!readline_init) {
		rl_readline_name = "GS+";
		rl_attempted_completion_function = rl_acf;

		using_history();
		stifle_history(HISTORY_SIZE);

		readline_init = 1;
	}

	cp = readline(prompt);
	if (!cp) return NULL;
	count = strlen(cp);
	if (count > sizeof(buffer) - 1) {
		free(cp);
		return "";
	}
	memcpy(buffer, cp, count);
	cleanup_buffer(buffer, count);
	free(cp);

	/* append to history, but only if unique from prev. entry */
	if (*buffer) {
		HIST_ENTRY *h = history_get(history_length);
		if (h == NULL || strcmp(buffer, h->line))
			add_history(buffer);
	}
	return buffer;
}

void x_readline_end(void) {
}
#elif defined(_WIN32)

#include <windows.h>

static int readline_init = 0;

#ifndef HISTORY_NO_DUP_FLAG
#define HISTORY_NO_DUP_FLAG 0x01
#endif

char *x_readline(const char *prompt) {
	static char buffer[1024];
	DWORD count = 0;
	BOOL ok;
	HANDLE h = GetStdHandle(STD_INPUT_HANDLE);

	if (!readline_init) {
		CONSOLE_HISTORY_INFO chi;
		DWORD mode;

		memset(&chi, 0, sizeof(chi));
		chi.cbSize = sizeof(CONSOLE_HISTORY_INFO);
		chi.HistoryBufferSize = HISTORY_SIZE;
		chi.NumberOfHistoryBuffers = 1; /* ???? */
		chi.dwFlags = HISTORY_NO_DUP_FLAG;
		SetConsoleHistoryInfo(&chi);

		mode = ENABLE_ECHO_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE;
		SetConsoleMode(h, mode);

		readline_init = 1;
	}

	ok = ReadConsole(h, buffer, sizeof(buffer), &count, NULL);
	if (!ok) return NULL;

	cleanup_buffer(buffer, count);
	return buffer;
}


#else

#include <unistd.h>

char *x_readline(const char *prompt) {
	static char buffer[1024];
	fputs(prompt, stdout);
	fflush(stdout);

	for(;;) {
		int ok = read(STDIN_FILENO, buffer, sizeof(buffer)-1);
		if (ok < 0) {
			if (ok == EINTR) continue;
			return NULL;
		}
		if (ok == 0) return NULL;
		cleanup_buffer(buffer, ok);

		return buffer;
	}
}

void x_readline_end(void) {

}
#endif
