#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

int main( int argc , char * argv[] )
{
	openlog(NULL, LOG_CONS || LOG_NDELAY || LOG_PID || LOG_PERROR, LOG_USER);//man syslog.
	if (argc != 3)
	{
		errno = EINVAL;
		syslog(LOG_ERR, "Invalid number of arguments (%d) . Enter like this: %s <location> <string> __%s", argc, argv[0], strerror(errno));
		//closelog();
		return 1;
	}

	FILE * write_file;
	write_file = fopen(argv[1], "w");
	if (write_file == NULL)
	{
		syslog(LOG_ERR, "Error in File Open. file: %s __%s", argv[1], strerror(errno));
		//closelog();
		return 1;
	}

	syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);

	int written = fwrite(argv[2], 1, strlen(argv[2]), write_file);
	if (written != strlen(argv[2]))
	{
		syslog(LOG_ERR, "Error in writing the file. __%s", strerror(errno));
		fclose(write_file);
	//	closelog();
		return 1;
	}

	fclose(write_file);
	
	return 0;
}

