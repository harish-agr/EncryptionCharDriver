/* OS416: Assignment 2 - Char() Device using ioctl() 
 * By: Zac Brown, Pintu Patel, Priya
 * November 3, 2013
 */

#ifndef CRYPTCTL_H
#define CRYPTCTL_H
#include <linux/ioctl.h>
 

#define QUERY_CREATE_PAIR _IO('q', 1)
#define QUERY_DESTORY_PAIR _IO('q', 2) 
#define QUERY_SET_SIZE _IOW('q',4,int *)
#define QUERY_SET_KEY _IOW('q',5,char *)
#define QUERY_GET_SIZE _IOR('q',6,int *)
#define QUERY_GET_KEY _IOR('q',7,char *)
 
#endif
