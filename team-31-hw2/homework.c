/* 
 * file:        homework.c
 * description: Skeleton code for CS 5600 Homework 2
 *
 * Peter Desnoyers, Northeastern CCIS, 2011
 * $Id: homework.c 530 2012-01-31 19:55:02Z pjd $
 */

#include <stdio.h>
#include <stdlib.h>
#include "hw2.h"

/********** YOUR CODE STARTS HERE ******************/

/*
 * Here's how you can initialize global mutex and cond variables
 */
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t C_barber_wake = PTHREAD_COND_INITIALIZER;
pthread_cond_t C_barber_ready = PTHREAD_COND_INITIALIZER;
pthread_cond_t C_customer_waiting = PTHREAD_COND_INITIALIZER;
pthread_cond_t C_haircut_done = PTHREAD_COND_INITIALIZER;
pthread_cond_t C_customer_leave = PTHREAD_COND_INITIALIZER;

#define CHAIR_NUM 4
#define CUSTOMER_NUM 10
// number of customers go to the shop
int total_customers = 0;
// number of customers being served and to be served
int ready_customers = 0;
// number of customers leave immediately
int leave_customers = 0;

//val for stastic in q3
void *timer = NULL;
void *in_shop_counter = NULL;
void *in_chair_counter = NULL;

/* the barber method
 */
void barber(void)
{
    pthread_mutex_lock(&m);
    while (1) {
        /* your code here */
        if(ready_customers == 0){    // no customer waiting for haircut
            printf("DEBUG: %f barber goes to sleep\n", timestamp());
            // barber goes to sleep, and wait for customer to wake him up
            pthread_cond_wait(&C_barber_wake, &m);
            // barber tells the customer he is ready
            pthread_cond_signal(&C_barber_ready);
            
        } else{
            // barber picks up a customer waiting in the chair
            pthread_cond_signal(&C_customer_waiting);
        }
        // barber starts haircut
        sleep_exp(1.2, &m);
        // barber finishes haircut
        pthread_cond_signal(&C_haircut_done);
        // barber waits for customer to leave
        pthread_cond_wait(&C_customer_leave, &m);
    }
    pthread_mutex_unlock(&m);
}

/* the customer method
 */
void customer(int customer_num)
{
    pthread_mutex_lock(&m);
    /* your code here */
    printf("DEBUG: %f customer %d enters shop\n", timestamp(), customer_num);
    total_customers ++;
    // if the barber is busy and there are no free chairs, 
    // customer leaves immediately
    if(ready_customers > CHAIR_NUM){
        printf("DEBUG: %f customer %d leaves shop\n", timestamp(), customer_num);
        leave_customers ++;
    } else{
		stat_timer_start(timer);//start counting the timer
        stat_count_incr(in_shop_counter); 
        ready_customers ++;
        // if this is the only customer in shop
        if(ready_customers == 1){
            // wake up the barber
            pthread_cond_signal(&C_barber_wake);
            printf("DEBUG: %f barber wakes up\n", timestamp());
            // wait for the barber ready for haircut
            pthread_cond_wait(&C_barber_ready, &m);
        } else{// if the barber is busy
            // grab a seat, and wait until barber is free
            pthread_cond_wait(&C_customer_waiting, &m);
        }
        printf("DEBUG: %f customer %d starts haircut\n", timestamp(), customer_num);
        stat_count_incr(in_chair_counter);
        // wait until barber finishes haircut
        pthread_cond_wait(&C_haircut_done, &m);
        stat_count_decr(in_chair_counter);
        // leave the shop after haircut
        ready_customers --;
        pthread_cond_signal(&C_customer_leave);
		stat_timer_stop(timer);
        printf("DEBUG: %f customer %d leaves shop\n", timestamp(), customer_num);
        stat_count_decr(in_shop_counter);
    }

    pthread_mutex_unlock(&m);
}

/* Threads which call these methods. Note that the pthread create
 * function allows you to pass a single void* pointer value to each
 * thread you create; we actually pass an integer (the customer number)
 * as that argument instead, using a "cast" to pretend it's a pointer.
 */

/* the customer thread function - create 10 threads, each of which calls
 * this function with its customer number 0..9
 */
void *customer_thread(void *context) 
{
    int customer_num = (int)context; 

    /* your code goes here */
    while(1){
        sleep_exp(10.0, NULL);
        customer(customer_num);
    }
    
    return 0;
}

/*  barber thread
 */
void *barber_thread(void *context)
{
    barber(); /* never returns */
    return 0;
}

void q2(void)
{
    /* to create a thread:
        pthread_t t; 
        pthread_create(&t, NULL, function, argument);
       note that the value of 't' won't be used in this homework
    */

    /* your code goes here */
    // create a barber thread
    pthread_t barber;
    pthread_create(&barber, NULL, *barber_thread, NULL);
    // create 10 customer threads
    int i;
    for(i = 0; i < CUSTOMER_NUM; i ++){
        pthread_t customer;
        pthread_create(&customer, NULL, &customer_thread, (void *)i);
    }
    wait_until_done();
}

/* For question 3 you need to measure the following statistics:
 *
 * 1. fraction of  customer visits result in turning away due to a full shop 
 *    (calculate this one yourself - count total customers, those turned away)
 * 2. average time spent in the shop (including haircut) by a customer 
 *     *** who does not find a full shop ***. (timer)
 * 3. average number of customers in the shop (counter)
 * 4. fraction of time someone is sitting in the barber's chair (counter)
 *
 * The stat_* functions (counter, timer) are described in the PDF. 
 */

void q3(void)
{
    /* your code goes here */
    timer = stat_timer();
    in_shop_counter = stat_counter();
    in_chair_counter = stat_counter();
    
	int i = 0;
	pthread_t _barber;
	pthread_create(&_barber, NULL, &barber_thread, NULL);
	for (i = 0; i< CUSTOMER_NUM; i++){
		pthread_t _customer;
		pthread_create(&_customer, NULL, &customer_thread, (void *)i);
	}
	wait_until_done();

    //calculate all required statistics
    double turned_away_fraction = leave_customers*1.0 / total_customers;
	double total_mean_time = stat_timer_mean(timer);
    double in_shop_mean_time = stat_count_mean(in_shop_counter);
	double total_mean_chair_time = stat_count_mean(in_chair_counter);
	printf("Fraction of customer visits result in turning away due to a full shop %f\n", turned_away_fraction);
	printf("Average time spent in the shop (including haircut) by a customer %f\n", total_mean_time);
	printf("Average number of customers in the shop %f\n", in_shop_mean_time);
	printf("Fraction of time someone is sitting in the barber's chair %f\n", total_mean_chair_time);
}
