/* mykernel2.c: your portion of the kernel
 *
 *	Below are procedures that are called by other parts of the kernel. 
 *	Your ability to modify the kernel is via these procedures.  You may
 *	modify the bodies of these procedures any way you wish (however,
 *	you cannot change the interfaces).  
 * 
 */

#include "aux.h"
#include "sys.h"
#include "mykernel2.h" 

#define TIMERINTERVAL 1	// in ticks (tick = 10 msec); was 100000
#define L 100
#define MAXINT 2147483647

/*	A sample process table. You may change this any way you wish.  
 */

static struct {
	int valid;		// is this entry valid: 1 = yes, 0 = no
	int pid;		// process ID (as provided by kernel)

	//For Proportional Policy
	float passval;
	float stride;
	float cpu_portion;
	int did_request;
} proctab[MAXPROCS];


/* FIFO Scheduling policy */
static struct {
	int queue[MAXPROCS]; 
	int startidx;
	int endidx;
}Queue;

/* LIFO Scheduling policy */
static struct {
	int stack[MAXPROCS];
	int pointer;
}Stack;

/* Round Robin policy */
static int startidx;

/* Request CPU time */
static float cpu_alloc;
static int num_reqs;
static int num_procs;

/* Allocate left over CPU to unrequested processes */
void AllocateRemainCPU();

/*	InitSched () is called when kernel starts up.  First, set the
 *	scheduling policy (see sys.h). Make sure you follow the rules
 *	below on where and how to set it.  Next, initialize all your data
 *	structures (such as the process table).  Finally, set the timer
 *	to interrupt after a specified number of ticks. 
 */

void InitSched ()
{
	int i;

	/* First, set the scheduling policy.  You should only set it
	 * from within this conditional statement.  While you are working
	 * on this assignment, GetSchedPolicy will return NOSCHEDPOLICY,
	 * and so the condition will be true and you may set the scheduling
	 * policy to whatever you choose (i.e., you may replace ARBITRARY). 
	 * After the assignment is over, during the testing phase, we will
	 * have GetSchedPolicy return the policy we wish to test, and so
	 * the condition will be false and SetSchedPolicy will not be
	 * called, thus leaving the policy to whatever we chose to test.  
	 */
	if (GetSchedPolicy () == NOSCHEDPOLICY) {	// leave as is
		SetSchedPolicy (PROPORTIONAL);		// set policy here
	}
		
	/* Initialize all your data structures here */
	for (i = 0; i < MAXPROCS; i++) {
		proctab[i].valid = 0;

		Queue.queue[i] = 0; // FIFO
		Stack.stack[i] = 0; // LIFO 

		//Proportional
		proctab[i].passval = 0;
		proctab[i].stride = 0;
		proctab[i].cpu_portion = 0;
		proctab[i].did_request = 0;
	}

	Queue.startidx = Queue.endidx = Stack.pointer = 0;
	startidx = num_reqs = cpu_alloc = num_procs = 0;
	
	/* Set the timer last */
	SetTimer (TIMERINTERVAL);
}


/*	StartingProc (pid) is called by the kernel when the process
 *	identified by pid is starting.  This allows you to record the
 *	arrival of a new process in the process table, and allocate
 *	any resources (if necessary). Returns 1 if successful, 0 otherwise.  
 */

int StartingProc (pid)
	int pid;			// process that is starting
{
	int i;

	for (i = 0; i < MAXPROCS; i++) {
		if (!proctab[i].valid) {
			proctab[i].valid = 1;
			proctab[i].pid = pid;

			proctab[i].passval = 0;
			proctab[i].did_request = 0;

            //sets the stride/cpu_portion of new process
            AllocateRemainCPU();

			if (GetSchedPolicy() == LIFO){
				Stack.stack[Stack.pointer++] = i;
				DoSched(); //process started; now schedule it

			}else if (GetSchedPolicy() == FIFO){
				Queue.queue[Queue.endidx++] = i;
				Queue.endidx = Queue.endidx % MAXPROCS;
			}
			num_procs++;
			return (1);
		}

	}

	Printf ("Error in StartingProc: no free table entries\n");
	return (0);
}
			

/*	EndingProc (pid) is called by the kernel when the process
 *	identified by pid is ending.  This allows you to update the
 *	process table accordingly, and deallocate any resources (if
 *	necessary). Returns 1 if successful, 0 otherwise.  
 */


int EndingProc (pid)
	int pid;			// process that is ending
{
	int i;

	/* Do the opposite we did in Starting the process.
	We don't care what the values are left in the stack/queue because
	we are use the pointer to help us keep track of insertion */
	if (GetSchedPolicy() == LIFO)
		Stack.pointer = (--Stack.pointer) % MAXPROCS;

	else if (GetSchedPolicy() == FIFO)
		Queue.startidx = (++Queue.startidx) % MAXPROCS;
	

	for (i = 0; i < MAXPROCS; i++) {
		if (proctab[i].valid && proctab[i].pid == pid) {
			proctab[i].valid = 0;

			//Proportional Policy
            if (proctab[i].did_request)
                num_reqs--;
                cpu_alloc -= proctab[i].cpu_portion;
            
			num_procs--;
			
            proctab[i].did_request = 0;
            proctab[i].cpu_portion = 0;
			proctab[i].passval = 0;
			proctab[i].stride = 0;
            
            AllocateRemainCPU();
			
			return (1);
		}
	}

	Printf ("Error in EndingProc: can't find process %d\n", pid);
	return (0);
}


/*	SchedProc () is called by kernel when it needs a decision for
 *	which process to run next.  It calls the kernel function
 *	GetSchedPolicy () which will return the current scheduling policy
 *	which was previously set via SetSchedPolicy (policy). SchedProc ()
 *	should return a process id, or 0 if there are no processes to run.  
 */

int SchedProc ()
{
	int i;
	int procid = 0;
    int minpass = MAXINT;
    
	switch (GetSchedPolicy ()) {

	case ARBITRARY:
		for (i = 0; i < MAXPROCS; i++) {
			if (proctab[i].valid) {
				return (proctab[i].pid);
			}
		}
		break;

	case FIFO:
		procid = Queue.queue[Queue.startidx];
		if (proctab[procid].valid) return proctab[procid].pid;
		break;

	case LIFO:
		procid = Stack.stack[Stack.pointer-1];
		if (proctab[procid].valid) return proctab[procid].pid;
		break;

	case ROUNDROBIN:
		for (i = 0; i < MAXPROCS; i++){
			int idx = (startidx++) % MAXPROCS;
			if (proctab[idx].valid) return proctab[idx].pid;
		}
		break;

	case PROPORTIONAL:
		//select the minimum pass value
		procid = -1;
		for (i = 0; i < MAXPROCS; i++){
			if (proctab[i].valid && proctab[i].passval < minpass && proctab[i].cpu_portion != 0){
				minpass = proctab[i].passval;
				procid = i; 
			}
		}
		if (procid != -1){
            //integer overflow handling
            if (proctab[procid].passval + proctab[procid].stride < 0){
                int d;
                for (d = 0; d < MAXPROCS; d++){
                    if (proctab[d].valid && proctab[d].cpu_portion != 0){
                        proctab[d].passval -= minpass;
                    }
                }
            }
            proctab[procid].passval += proctab[procid].stride;
            //REMOVE
            DPrintf("%d\n", proctab[procid].pid);
			return proctab[procid].pid;
		}
		break;

	}
	
	return (0);
}


/*	HandleTimerIntr () is called by the kernel whenever a timer
 *	interrupt occurs.  
 */

void HandleTimerIntr ()
{
	SetTimer (TIMERINTERVAL);

	switch (GetSchedPolicy ()) {	// is policy preemptive?

	case ROUNDROBIN:		// ROUNDROBIN is preemptive
	case PROPORTIONAL:		// PROPORTIONAL is preemptive

		DoSched ();		// make scheduling decision
		break;

	default:			// if non-preemptive, do nothing
		break;
	} 
}

void AllocateRemainCPU()
{
	float equal_cpu_util = (100 - cpu_alloc)/(float)(num_procs - num_reqs);
	int d;
	//equally divide the cpu util among unrequested processes
	for(d = 0; d < MAXPROCS; d++){
		if (proctab[d].valid && !proctab[d].did_request){
            if (equal_cpu_util == 0)
                proctab[d].stride = L;
            else
                proctab[d].stride = L/equal_cpu_util;
            proctab[d].cpu_portion = equal_cpu_util;
		}
	}
}
/*	MyRequestCPUrate (pid, n) is called by the kernel whenever a process
 *	identified by pid calls RequestCPUrate (n). This is a request for
 *	n% of CPU time, i.e., requesting a CPU whose speed is effectively
 *	n% of the actual CPU speed.  Roughly n out of every 100 quantums
 *	should be allocated to the calling process.  n must be greater than
 *	0 and must be less than or equal to 100. MyRequestCPUrate (pid, n)
 *	should return 0 if successful, i.e., if such a request can be
 *	satisfied, otherwise it should return -1, i.e., error (including if
 *	n < 1 or n > 100).  If MyRequestCPUrate (pid, n) fails, it should
 *	have no effect on scheduling of this or any other process, i.e., AS
 *	IF IT WERE NEVER CALLED.
 */

int MyRequestCPUrate (pid, n)
	int pid;			// process whose rate to change
	int n;				// percent of CPU time
{
	/* your code here */
	if (n < 1 || n > 100)  return -1;
	int i;

	for (i = 0; i < MAXPROCS; i++){
		if (proctab[i].valid && proctab[i].pid == pid){
			if (proctab[i].did_request){
				int tmp_alloc = cpu_alloc - proctab[i].cpu_portion;
				if (tmp_alloc + n > 100.0)
					return -1;
			}
		}
	}
	
	for (i = 0; i < MAXPROCS; i++){
		//find the right process to allocate cpu time for 
		if (proctab[i].valid && proctab[i].pid == pid){
			proctab[i].stride = L/(float)n;
			
            //what if the process has already requested before?
			if (proctab[i].did_request)
                cpu_alloc -= proctab[i].cpu_portion;

            cpu_alloc += n;
            
            proctab[i].cpu_portion = (float)n;
			proctab[i].did_request = 1;
			++num_reqs;
			
            AllocateRemainCPU();

		}
        proctab[i].passval = 0;
	}

	return (0);
}
