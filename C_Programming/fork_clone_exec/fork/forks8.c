#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>

#define CHILD 0

pid_t cPid, cPid1, ret, ret1;

void sighand(int signum)
{
        ret = waitpid(cPid, NULL, 0);
	printf("%s: Pid of child cleanup in async mode %d \n", __func__,ret); 
}
int main()
{
	struct sigaction act;
	/* first child */
	cPid = fork();
	if (cPid == CHILD) {
		printf(" child: My pid = %d \n", getpid());
		exit(100);
	} else 
	       {
	
                 signal(SIGCHLD,sighand);
		/* forking 2nd child */
		cPid1 = fork();
		if (cPid1 == CHILD) {
			printf(" child: My pid = %d \n", getpid());
			exit(200);
		} else {
			/* synchronous cleanup */
			ret1 = waitpid(cPid1, NULL, 0);
			printf("parent: pid of child cleaned up in sync mode %d\n", ret1);
			while(1);
		}
	}
	return 0;
}
