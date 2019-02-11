#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

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
	int ok = 0;
	const char *cp;

	if (!el) {
		hist = history_init();
		history(hist, &ev, H_SETSIZE, 50);
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
	cp = el_gets(el, &ok);
	el_prompt = NULL;
	if (ok <= 0) return NULL;
	if (ok > sizeof(buffer) - 1) return "";


	memcpy(buffer, cp, ok);
	while (ok) {
		unsigned c = buffer[ok-1];
		if (c == ' ' || c == '\r' || c == '\n') {
			--ok;
			continue;
		}
		break;
	}
	buffer[ok] = 0;

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
	int ok;

	if (!readline_init) {
		rl_readline_name = "GS+";
		rl_attempted_completion_function = rl_acf;

		using_history();
		stifle_history(50);

		readline_init = 1;
	}

	cp = readline(prompt);
	if (!cp) return NULL;
	ok = strlen(cp);
	if (ok > sizeof(buffer) - 1) {
		free(cp);
		return "";
	}
	memcpy(buffer, cp, ok);
	while (ok) {
		unsigned c = buffer[ok-1];
		if (c == ' ' || c == '\r' || c == '\n') {
			--ok;
			continue;
		}
		break;
	}
	buffer[ok] = 0;
	free(cp);

	/* append to history, but only if unique from prev. entry */
	if (*buffer) {
		HIST_ENTRY *h = history_get(history_length-1);
		if (h == NULL || strcmp(buffer, h->line))
			add_history(buffer);
	}
	return buffer;
}

void x_readline_end(void) {
}
#else

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
		while (ok) {
			unsigned c = buffer[ok-1];
			if (c == ' ' || c == '\r' || c == '\n') {
				--ok;
				continue;
			}
			break;
		}
		buffer[ok] = 0;

		return buffer;
	}
}

void x_readline_end(void) {

}
#endif
