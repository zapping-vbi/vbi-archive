#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

/* A codelet to test if zapping_setup_fb can inherit a file descriptor
   if we execute it directly, as a child process and via consolehelper.
   zapping_setup_fb is expected to complain about fd not referring to
   a video device, but not about fd being invalid (EBADF). */

int
main (void)
{
	char str[256];
	char *argv[9];
	int fd;
	
	fd = open ("/dev/null", O_RDWR, 0666);

	snprintf (str, sizeof (str), "%d", fd);

	argv[0] = "zapping_setup_fb";
	argv[1] = "-c";
	argv[2] = "-f";
	argv[3] = str;
	argv[4] = "-D";
	argv[5] = ":0.0";
	argv[6] = "-S";
	argv[7] = "0";
	argv[8] = NULL;

	if (1) {
		pid_t pid;

		pid = fork ();

		switch (pid) {
		case -1:
			perror ("fork");
			exit (EXIT_FAILURE);

		case 0: /* in child */
			/* Does not return on success. */
			execvp (argv[0], argv);
			exit (EXIT_FAILURE);

		default: /* in parent */
			fprintf (stderr, "fork ok\n");
			exit (EXIT_SUCCESS);
		}
	} else {
		/* Does not return on success. */
		execvp (argv[0], argv);
		exit (EXIT_FAILURE);
	}
}
