/* A Round Robin Scheduler Implementation */
/* */

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "proc-common.h"
#include "request.h"
/* Compile-time parameters. */
#define SCHED_TQ_SEC 2 /* time quantum */
#define TASK_NAME_SZ 60 /* maximum size for a task's name */
#define SHELL_EXECUTABLE_NAME "shell" /* executable for shell */
/*a simple linked list for the RR scheduler*/
/* 1 (one) for high priority 0 (zero) low priority*/
int nproc,idcnt;
struct list *head, *prev;
/*since its Round, last->first */
struct list
{
	pid_t pid;
	int s_id;
	char *name;
	struct list *next;
	int priority;
};
static void do_task_high(int id){
	struct list *temp=head;
	while (temp->s_id != id)
	temp = temp->next;
	temp->priority = 1;
}
static void do_task_low(int id){
	struct list *temp=head;
	while (temp->s_id != id)
	temp = temp->next;
	temp->priority = 0;
}
void add_to_list(pid_t a,int b, int c,char *onoma){
	// int is_shell;
	struct list *temp=head;
	struct list *new = (struct list *)malloc(sizeof(struct list));
	new->pid=a;
	new->s_id=b;
	new->name=onoma;
	new->priority=0;
	
	if (head==NULL)
	{
		head=new;
		new->next=NULL ;
		if (b==c){
			new->next=head;
			prev=new;
		}
	}
	else
	{
		while (temp->next!=NULL)
		temp =temp->next;
		temp->next=new;
		new->next=NULL;
		// circular list
		if (b==c){
			new->next=head;
			prev=new;
		}
	}
}
static void roll_around(){
	struct list *temp1,*temp2;
	if (head->next==NULL) { return;}
	temp1=head->next;
	temp2=head;
	while ((temp1->priority==0) && (temp1 != temp2)){
		temp1=temp1->next;
	}
	head=temp1;
	/*head=head->next;*/
	if (prev->next!=head)
	prev=prev->next;
}
void delete_process(int p_id)
{
	if (head->pid!=p_id)
	{
		
		/*in case there is a process terminated (which is not executed at the moment)*/
		struct list *t2,*t1;
		t1=head;
		while (t1->next->pid!=p_id)
		t1=t1->next;
		t2=t1->next;
		t1->next=t2->next;
		free(t2);
		return;
	}
	head=head->next;
	free(prev->next);
	prev->next=head;
}
/* Print a list of all tasks currently being scheduled. */
static void
sched_print_tasks(void)
{
	struct list *temp;
	temp=head;
	if (temp->next==NULL)
	{
		printf("\t PROCESS:%s ID: %d PID:[%d] Priority: %d\n",temp->name,temp->s_id,temp->pid,temp->priority);
		return ;
	}
	while(temp->next != head ) {
		printf("\t PROCESS:%s ID: %d PID:[%d] Priority: %d\n",temp->name,temp->s_id,temp->pid,temp->priority);
		temp=temp->next;
	}
	printf("\t PROCESS:%s ID: %d PID:[%d] Priority: %d\n",temp->name,temp->s_id,temp->pid,temp->priority);
}
/* Send SIGKILL to a task determined by the value of its
* scheduler-specific id.
*/
static int
sched_kill_task_by_id(int id)
{
	int pid;
	struct list *temp1, *temp2;
	temp1=head;
	temp2=head; /* I find the id-process and the previous one*/
	while (temp1->s_id != id){
		temp1=temp1->next;
	}
	while (temp2->next != temp1) {
		temp2=temp2->next;
	}
	pid=temp1->pid;
	/* killing the task*/
	nproc--;
	kill(pid,SIGKILL);
	return 2;
}
/* Create a new task. */
static void
sched_create_task(char *executable)
{
	struct list *temp;
	pid_t pid;
	pid=fork();
	/* execve */
	if (pid == 0){
		char *newargv[] = { executable, NULL, NULL, NULL };
		char *newenviron[] = { NULL };
		raise(SIGSTOP);
		execve(executable, newargv, newenviron);
		/* execve() only returns on error */
		perror("execve");
		exit(1);
	}
	else {
		struct list *new = (struct list *)malloc(sizeof(struct list));
		char buff[10];
		strcpy(buff,executable);
		new->pid=pid;
		new->s_id=idcnt;
		new->name= buff;
		new->priority=0;
		temp=head->next;
		head->next=new;
		if (temp!=NULL)
		new->next=temp;
		else
		new->next=head;
		nproc++; idcnt++;
	}
}
/* Process requests by the shell. */
static int
process_request(struct request_struct *rq)
{
	switch (rq->request_no) {
	case REQ_PRINT_TASKS:
		sched_print_tasks();
		return 0;
	case REQ_KILL_TASK:
		return sched_kill_task_by_id(rq->task_arg);
	case REQ_EXEC_TASK:
		sched_create_task(rq->exec_task_arg);
		return 0;
	case REQ_HIGH_TASK:
		do_task_high(rq->task_arg);
		return 0;
	case REQ_LOW_TASK:
		do_task_low(rq->task_arg);
		return 0;
	default:
		return -ENOSYS;
	}
}
/*
* SIGALRM handler
*/
static void
sigalrm_handler(int signum)
{
	/* Paei to kvanto xronou dike mou. Pame next */
	kill(head->pid, SIGSTOP);
}
/*
* SIGCHLD handler
*/
static void
sigchld_handler(int signum)
{
	/* Either the child died, or it stopped cause of the alarm-handler */ 
	/* Case 1, put it out of the list */
	/* Case 2, SIGCONT the next process */
	/*To paidi eite pe8ane, eite stamathse logw tou alarm-handler */
	/* Case 1, to vgazw apo th lista. */
	/* Case 2, kanw SIGCONT sthn epomenh diergasia */
	int p, status;
	for (;;) {
		p = waitpid(-1, &status, WUNTRACED | WNOHANG);
		if (p < 0) {
			perror("waitpid"); exit(1);
		}
		if (p == 0)
		break;
		explain_wait_status(p, status);
		if (WIFEXITED(status) || WIFSIGNALED(status)){
			/* A child has died */
			delete_process(p);
			alarm(SCHED_TQ_SEC);
			kill(head->pid ,SIGCONT);
		}
		if (WIFSTOPPED(status)){
			/* A child has stopped due to SIGSTOP/SIGTSTP, etc */
			/* take the next pointer */
			//kill(head->pid, SIGSTOP);
			roll_around();
			// printf("AGAIN: \t[%d,%d : %d]\n",head->s_id,head->pid,head->priority);
			alarm(SCHED_TQ_SEC);
			kill(head->pid, SIGCONT);
		}
	}
}
/* Disable delivery of SIGALRM and SIGCHLD. */
static void
signals_disable(void)
{
	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	sigaddset(&sigset, SIGCHLD);
	if (sigprocmask(SIG_BLOCK, &sigset, NULL) < 0) {
		perror("signals_disable: sigprocmask");
		exit(1);
	}
}
/* Enable delivery of SIGALRM and SIGCHLD. */
static void
signals_enable(void)
{
	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	sigaddset(&sigset, SIGCHLD);
	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) < 0) {
		perror("signals_enable: sigprocmask");
		exit(1);
	}}
/* Install two signal handlers.
* One for SIGCHLD, one for SIGALRM.
* Make sure both signals are masked when one of them is running.
*/
static void
install_signal_handlers(void)
{
	sigset_t sigset;
	struct sigaction sa;
	sa.sa_handler = sigchld_handler;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGCHLD);
	sigaddset(&sigset, SIGALRM);
	sa.sa_mask = sigset;
	if (sigaction(SIGCHLD, &sa, NULL) < 0) {
		perror("sigaction: sigchld");
		exit(1);
	}
	sa.sa_handler = sigalrm_handler;
	if (sigaction(SIGALRM, &sa, NULL) < 0) {
		perror("sigaction: sigalrm");
		exit(1);
	}
	/*
* Ignore SIGPIPE, so that write()s to pipes
* with no reader do not result in us being killed,
* and write() returns EPIPE instead.
*/
	if (signal(SIGPIPE, SIG_IGN) < 0) {
		perror("signal: sigpipe");
		exit(1);
	}
}
static void
do_shell(char *executable, int wfd, int rfd)
{
	char arg1[10], arg2[10];
	char *newargv[] = { executable, NULL, NULL, NULL };
	char *newenviron[] = { NULL };
	sprintf(arg1, "%05d", wfd);
	sprintf(arg2, "%05d", rfd);
	newargv[1] = arg1;
	newargv[2] = arg2;
	raise(SIGSTOP);
	execve(executable, newargv, newenviron);
	/* execve() only returns on error */
	perror("scheduler: child: execve");
	exit(1); }
/* Create a new shell task.
*
* The shell gets special treatment:
* two pipes are created for communication and passed
* as command-line arguments to the executable.
*/
static void
sched_create_shell(char *executable, int *request_fd, int *return_fd,pid_t *p)
{
	// pid_t p;
	int pfds_rq[2], pfds_ret[2];
	if (pipe(pfds_rq) < 0 || pipe(pfds_ret) < 0) {
		perror("pipe");
		exit(1);
	}
	*p = fork();
	if (*p < 0) {
		perror("scheduler: fork");
		exit(1);
	}
	if (*p == 0) {
		/* Child */
		close(pfds_rq[0]);
		close(pfds_ret[1]);
		do_shell(executable, pfds_rq[1], pfds_ret[0]);
		assert(0);
	}
	/* Parent */
	close(pfds_rq[1]);
	close(pfds_ret[0]);
	*request_fd = pfds_rq[0];
	*return_fd = pfds_ret[1];
}
static void
shell_request_loop(int request_fd, int return_fd)
{
	int ret;
	struct request_struct rq;
	/*
* Keep receiving requests from the shell.
*/
	for (;;) {
		if (read(request_fd, &rq, sizeof(rq)) != sizeof(rq)) {
			perror("scheduler: read from shell");
			fprintf(stderr, "Scheduler: giving up on shell request processing.\n");
			break;
		}
		signals_disable();
		ret = process_request(&rq);
		signals_enable();
		if (write(return_fd, &ret, sizeof(ret)) != sizeof(ret)) {
			perror("scheduler: write to shell");
			fprintf(stderr, "Scheduler: giving up on shell request processing.\n");
			break;
		}
	}
}
int main(int argc, char *argv[])
{
	int i,pid;
	pid_t shell_pid;
	shell_pid=0;
	/* Two file descriptors for communication with the shell */
	static int request_fd, return_fd;
	/* Create the shell. */
	sched_create_shell(SHELL_EXECUTABLE_NAME, &request_fd, &return_fd,&shell_pid);
	/* TODO: add the shell to the scheduler's tasks */
	/*
* For each of argv[1] to argv[argc - 1],
* create a new child process, add it to the process list.
*/
	idcnt=0;
	nproc = 0; /* number of proccesses goes here */
	/* create the child-processes to be scheduled */
	add_to_list(shell_pid,nproc,argc-1,SHELL_EXECUTABLE_NAME);
	nproc++;
	idcnt++;
	for (i=0; i<argc-1; i++){
		pid=fork();
		/* execve */
		if (pid == 0){
			char executable[] = "prog";
			char *newargv[] = { executable, NULL, NULL, NULL };
			char *newenviron[] = { NULL };
			raise(SIGSTOP);
			execve(executable, newargv, newenviron);
			/* execve() only returns on error */
			perror("execve");
			exit(1);
		}
		else {
			add_to_list(pid,idcnt,argc-1,"prog");
			nproc++;
			idcnt++;
		}
	}
	if (nproc == 0) { fprintf(stderr, "Scheduler: No tasks. Exiting...\n");
		exit(1);
	}
	/* Wait for all children to raise SIGSTOP before exec()ing. */
	wait_for_ready_children(nproc);
	/* Install SIGALRM and SIGCHLD handlers. */
	install_signal_handlers();
	alarm(SCHED_TQ_SEC);
	kill(head->pid,SIGCONT);
	shell_request_loop(request_fd, return_fd);
	/* Now that the shell is gone, just loop forever
* until we exit from inside a signal handler.
*/
	while (pause())
	;
	/* Unreachable */
	fprintf(stderr, "Internal error: Reached unreachable point\n");
	return 1;
}