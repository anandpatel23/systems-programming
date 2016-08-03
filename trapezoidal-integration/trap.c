/*  Purpose: Calculate definite integral using the trapezoidal rule.
 *
 * Input:   a, b, n
 * Output:  Estimate of integral from a to b of f(x)
 *          using n trapezoids.
 *
 * Author: Naga Kandasamy, Michael Lui
 * Date: 6/22/2016
 *
 * Compile: gcc -o trap trap.c -lpthread -lm -std=c99
 * Usage:   ./trap
 *
 *
 * Note:    The function f(x) is hardwired.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <pthread.h>

#define LEFT_ENDPOINT 5
#define RIGHT_ENDPOINT 1000
#define NUM_TRAPEZOIDS 100000000
#define NUM_THREADs 2 /* Number of threads to run. */


/*------------------------------------------------------------------
 * Function:    func
 * Purpose:     Compute value of function to be integrated
 * Input args:  x
 * Output: (x+1)/sqrt(x*x + x + 1)
 */
__attribute__((const)) float func(float x) 
{
	return (x + 1)/sqrt(x*x + x + 1);
}  


double compute_gold(float, float, int, float (*)(float));
void *compute_using_pthreads(void *inputArgs);
typedef struct threadArgStruct threadArgs;

struct threadArgStruct{
	float a;
	float b;
	int n;
	float h;
	float (*f)(float);
	double ans;
	float exTime;
};

int main(int argc, char *argv[])
{
	int n = NUM_TRAPEZOIDS;
	float a = LEFT_ENDPOINT;
	float b = RIGHT_ENDPOINT;

	pthread_t threadManager;
	pthread_t workerThread[NUM_THREADs];
	threadArgs *tThread;

	/* single threaded timing start */
	clock_t start = clock(), diff;
		
	double reference = compute_gold(a, b, n, func);
	printf("Reference solution computed on the CPU = %f \n", reference);
	diff = clock() - start;
	int msec = diff * 1000 / CLOCKS_PER_SEC;
	printf("Time taken %d milliseconds\n", msec%1000);	
	/* single threaded timing end  */

	int i;

	tThread = (threadArgs *) malloc(sizeof(threadArgs));
	tThread -> a = a;
	tThread -> b = b;
	tThread -> n = n;
	tThread -> h = (b-a)/(float)n;
	tThread -> f = func;


	clock_t start2 = clock(), diff2;

	for(i = 0; i < NUM_THREADs; i++){

		if(pthread_create(&workerThread[i], NULL, compute_using_pthreads, (void *) tThread) != 0){
			printf("Error within pthread_create.\n");
			return -1;
		}
	}
	
	double *trapInt;
	double sum = 0;
	for(i = 0; i < NUM_THREADs; i++){
		pthread_join(workerThread[i], &trapInt);
		sum += *trapInt;
	}
	sum = sum/NUM_THREADs;
		
	diff2 = clock() - start2;	
	int msec2 = diff2 * 1000 / CLOCKS_PER_SEC;	

	printf("Pthreads compution solution: %f\n", sum);
	printf("Time taken %d milliseconds\n",(msec2%1000)/NUM_THREADs);
	pthread_exit((void *) threadManager);
} 



/*------------------------------------------------------------------
 * Function:    Trap
 * Purpose:     Estimate integral from a to b of f using trap rule and
 *              n trapezoids
 * Input args:  a, b, n, f
 * Return val:  Estimate of the integral 
 */
double compute_gold(float a, float b, int n, float(*f)(float))
{
	float h = (b-a)/(float)n; /* 'Height' of each trapezoid. */

	double integral = (f(a) + f(b))/2.0;
	
	for (int k = 1; k <= n-1; k++) 
		integral += f(a+k*h);
	
	integral = integral*h;
	
	return integral;
}  

// struct thread_data thread_data_array[NUM_THREADs];

void *compute_using_pthreads(void *inputArgs)
{
	
	/* numerical integration inputs */
	double *trapInt = malloc(sizeof(double));
	*trapInt = 0;
	threadArgs *inputs = (threadArgs *) inputArgs;
	
	/* assign inputs */
	float a = inputs -> a;
	float b = inputs -> b;
	int n = inputs -> n;
	float h = inputs -> h;
	float(*f)(float) = inputs -> f;
	
	int j;
		
	*trapInt = (f(a) + f(b))/2.0;

		
	for(j = 1; j <= n-1; j++){
		*trapInt += f(a+j*h);
	}
	
	*trapInt = *trapInt*h;
		
	return trapInt;
}
