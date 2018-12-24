// Project 1: Abenezer Wudenhe, awudenh; Abhishek Arya, aarya



//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2016
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for Processor Container
//
////////////////////////////////////////////////////////////////////////

#include "processor_container.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>



// Linked list structure for threads
struct thread_l {
  pid_t tid;
  struct task_struct* tsk;
  struct thread_l* next;
};
typedef struct thread_l thread_l;


// Linked list structure for containers
struct container{
  struct processor_container_cmd* cmd;
  thread_l* tids;
  //thread_l* t_current;
  long long unsigned int cid;
  struct container* next;
};
typedef struct container container;
typedef struct processor_container_cmd processor_container_cmd;


container* c_list = NULL;
DEFINE_MUTEX(lock);


void Trim(long long unsigned int cid){
  container* temp = c_list;
  container* c_prev = NULL;
  

  while((temp != NULL)){
    //REMOVE THIS CONTAINER, IT IS NOT NEEDED ANYMORE
    if((temp->tids == NULL)){
      printk("\tREMOVING CONTAINER [%llu]\n", temp->cid);

      // Container is at the front
      if(c_prev == NULL){
	c_list = c_list->next;
	temp->next = NULL;
	temp->cmd = NULL;
	//temp->t_current = NULL;
	kfree(temp);
      }

      // Container is in the middle of other conatiners
      else if((c_prev != NULL) && (temp->next != NULL)){
	c_prev->next = c_prev->next->next;
	temp->next = NULL;
	temp->cmd = NULL;
	//temp->t_current = NULL;
	kfree(temp);
      }

      // Container is at the end of the container list
      else if((c_prev != NULL) && (temp->next == NULL)){
	c_prev->next = NULL;
	c_prev->cmd = NULL;
	//temp->t_current = NULL;
	kfree(temp);
      }

      // Failed to implement a way to remove container
      else { printk("\n!!! CANT IDENTIFY HOW TO REMOVE CONTAINER !!!\n"); }
    }
  
    c_prev = temp;
    temp = temp->next;
  }

}


/**
 * Delete the task in the container.
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), 
 */
int processor_container_delete(struct processor_container_cmd __user *user_cmd)
{
  //int count = 0;
  container* temp = c_list;
  thread_l* t_current = NULL;
  thread_l* t_prev = NULL;
  struct task_struct* tsk = current;
  pid_t tid = tsk->pid;
  processor_container_cmd* temp_cmd=(processor_container_cmd*)kmalloc(sizeof(processor_container_cmd), GFP_KERNEL );
  long long unsigned int cid = 0;
  int found = 0;
  copy_from_user(temp_cmd, user_cmd, sizeof(processor_container_cmd));
  cid = temp_cmd->cid;
  
  
  printk("=====Entering Deletion=====\n");
  printk("Deleting tid[%u] in C[%llu]\n", tid, cid);
  

  mutex_lock(&lock);
  
    
  while(temp != NULL){
    if(temp->cid == cid){     // Found the container we are looking for
      
      t_current = temp->tids;
      
      while((t_current != NULL) && (found == 0)){
	if((t_current->tid == tid) && (found == 0)){

	  // Deleteing the first thread in the list
	  if(t_prev == NULL){
	    if(t_current->next != NULL){
	      temp->tids = temp->tids->next;
	      t_current->next = NULL;
	      t_current->tsk = NULL;
	      kfree(t_current);
	      printk(" DELETE: tid[%u] woken up by tid[%u]\n", temp->tids->tid, tsk->pid);
	      wake_up_process(temp->tids->tsk); // wakeup the next thread in the list as well
	      //temp->t_current = temp->tids;
	    } else {
	      temp->tids = NULL;
	      t_current->tsk = NULL;
	      kfree(t_current);
	      printk(" DELETE: No task woken up by tid[%u]\n", tsk->pid);
	      //temp->t_current = NULL;
	    }
	  }

	  // Delete thread in middle of other threads
	  else if((t_current->next != NULL) && (t_prev != NULL)){
	    t_prev->next = t_current->next;
	    t_current->next = NULL;
	    t_current->tsk = NULL;
	    kfree(t_current);
	    printk(" DELETE: tid[%u] woken up by tid[%u]\n", t_prev->next->tid, tsk->pid);
	    wake_up_process(t_prev->next->tsk); // Wake up next task in line
	    //temp->t_current = t_prev->next;
	  }
	  
	  
	  // Delete thread at the end of the list
	  else if((t_prev != NULL) && (t_current->next == NULL)){
	    t_prev->next = NULL;
	    t_current->tsk = NULL;
	    kfree(t_current);
	    printk(" DELETE: tid[%u] woken up by tid[%u]\n", temp->tids->tid, tsk->pid);
	    wake_up_process( temp->tids->tsk ); // Waking up process at the front of the list
	    //temp->t_current = temp->tids;
	  }

	  // I failed to implement a deleiton method
	  else{printk("\n!!! FAILED TO IDENTIFY THREAD DELEITON METHOD !!!\n");}

	  found = 1;
	}
	t_prev = t_current;
	t_current = t_current->next;
      }
    }

    temp = temp->next;
  }

  // make sure all containers have threads to manage
  Trim(cid);


  mutex_unlock(&lock);
  printk(" DELETE: unlocked tid[%u]\n", tsk->pid);    
  printk("===== DONE [%u]=====\n", tsk->pid);

  // AFTER DELETING THREAD FROM CONTAINER, THE CURRENT THREAD IS NO LONGERMANAGED BY THE CONTAINER
  // SO SWITCH WILL NOT WAKE THE CURRENT THREAD, SO SIAD THREAD WILL NOT BE PUT BACK TO SLEEP
  // HOWEVER THE LOCK WILL BE FREED AS YOU ARE DISSASOCIATED FROM THE CONTAINERS

  ///*

  //set_current_state(TASK_RUNNING);
  //*/
  
  return 0;
}

/**
 * Create a task in the corresponding container.
 * external functions needed:
 * copy_from_user(), mutex_lock(), mutex_unlock(), set_current_state(), schedule()
 * 
 * external variables needed:
 * struct task_struct* current  
 */
int processor_container_create(struct processor_container_cmd __user *user_cmd)
{

  int found = 0;
  container* temp = c_list;
  struct task_struct* tsk = current;
  thread_l* t_temp = NULL;
  int count = 1;
  processor_container_cmd* temp_cmd = (processor_container_cmd*) kmalloc(sizeof(processor_container_cmd), GFP_KERNEL );
  copy_from_user(temp_cmd, user_cmd, sizeof(processor_container_cmd));
  

  //printk("===== CREATE [%u]=====\n", tsk->pid);
  

  //GET THE LOCK TO EDIT THE LIST IN THE FIRST PLACE
  
  set_current_state(TASK_INTERRUPTIBLE);
  mutex_lock(&lock);
  
      
  // No Containers in the list
  if(c_list == NULL){
    
    c_list = (container*) kmalloc(sizeof(container), GFP_KERNEL);
    c_list->cmd = temp_cmd;
    c_list->cid = temp_cmd->cid;
    c_list->next = NULL;
    
    c_list->tids = (thread_l*) kmalloc(sizeof(thread_l), GFP_KERNEL);
    c_list->tids->tid = tsk->pid;
    c_list->tids->tsk = tsk;
    c_list->tids->next = NULL;
            
    //printk(" CREATE: added tid[%u] in cid[%llu][0]\n", tsk->pid, temp_cmd->cid);
    set_current_state(TASK_RUNNING);
    found = 1;    
  }
  
  // Containers exist, need to check for one we want
  else if(c_list != NULL){
    while(temp != NULL){
      if(temp->cid == temp_cmd->cid){
	
	t_temp = temp->tids;
	while((t_temp != NULL) && (t_temp->next != NULL)){
	  t_temp = t_temp->next;
	  count += 1;
	}
	
	t_temp->next = (thread_l*) kmalloc(sizeof(thread_l), GFP_KERNEL);
	t_temp->next->tid = tsk->pid;
	t_temp->next->tsk = tsk;
	t_temp->next->next = NULL;

	//printk("\tAdded tid[%u] to cid[%llu][%d]\n", tsk->pid, temp_cmd->cid, count);
	  
	mutex_unlock(&lock);
	//printk(" CREATE: unlocked tid[%u]\n", tsk->pid);

	if(temp->tids->tid != tsk->pid){
	  //printk(" CREATE:  tid[%u] set to sleep\n", tsk->pid );
	  set_current_state(TASK_INTERRUPTIBLE);
	  schedule();
	  set_current_state(TASK_RUNNING);
	  //printk(" CREATE:  tid[%u] woken up\n", tsk->pid );
	}
	 
	
	//printk(" CREATE: locking tid[%u]\n", tsk->pid);
	mutex_lock(&lock);
	//printk(" CREATE: locked tid[%u]\n", tsk->pid);
      	
	found = 1;
	temp = NULL;
      } else {
	temp = temp->next;
      }
    }
    
    // Container Does not exist, so creating a new container
    if(found == 0){
      //printk("\tContainer Does not exist\n");
      
      temp = c_list;
      while(temp->next != NULL){temp = temp->next;}
      
      //printk("\tCreating new container\n");
      temp->next = (container*) kmalloc(sizeof(container), GFP_KERNEL);
      temp->next->cmd = temp_cmd;
      temp->next->cid = temp_cmd->cid;
      temp->next->next = NULL;
      
      temp->next->tids = (thread_l*) kmalloc(sizeof(thread_l), GFP_KERNEL);
      temp->next->tids->tid = tsk->pid;
      temp->next->tids->tsk = tsk;
      temp->next->tids->next = NULL;
      
      //temp->next->t_current = temp->next->tids;
      
      //printk(" CREATE: added tid[%u] in cid[%llu][0]\n", tsk->pid, temp_cmd->cid);
      set_current_state(TASK_RUNNING);
      
  
    }    
  }

  // AFTER CREATING A NEW ENTRY IN THE LIST; UNLOCK, SLEEP, WAKEUP AGAIN 

  mutex_unlock(&lock);
  //printk(" CREATE: unlocked tid[%u]\n", tsk->pid);  
  //printk("===== END CREATE [%u]=====\n", tsk->pid);
  
  
  return 0;
}

/**
 * switch to the next task in the next container
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), set_current_state(), schedule()
 */

int processor_container_switch(struct processor_container_cmd __user *user_cmd)
{
  int found = 0;
  container* temp = NULL;
  thread_l* t_end = NULL;
  thread_l* t_current = NULL;
  struct task_struct* tsk = current;
  pid_t tid = tsk->pid;
  long long unsigned int cid = 0;
  processor_container_cmd* temp_cmd=(processor_container_cmd*)kmalloc(sizeof(processor_container_cmd), GFP_KERNEL );
  
  copy_from_user(temp_cmd, user_cmd, sizeof(processor_container_cmd));
  cid = temp_cmd->cid;
  
  set_current_state(TASK_INTERRUPTIBLE);
  mutex_lock(&lock);

  temp = c_list;
  while((temp != NULL) && (found == 0)){
    if(temp->tids->tid == tid){
      //printk(" SWITCH: Found tid[%u] at cid[%llu]\n", tid, temp->cid);
      if(temp->tids->next == NULL){
	//printk(" SWITCH: No other threads to switch too :(\n");
	set_current_state(TASK_RUNNING);
	found = 1;
      } else {
	//move thread to end of the list
	t_current = temp->tids;
	temp->tids = temp->tids->next;	
	t_end = temp->tids; while(t_end->next != NULL){ t_end = t_end->next;}
	t_end->next = t_current;
	t_current->next = NULL;

	//printk(" SWITCH: [%u]->[%u]\n", t_current->tid, temp->tids->tid);
	
	//wake up next task
	wake_up_process(temp->tids->tsk);

	//unlock 
	mutex_unlock(&lock);
	//printk(" SWITCH: unlocked tid[%u]\n", tid);

	//sleep current task
	//printk(" SWITCH:  tid[%u] set to sleep\n", tsk->pid );
	set_current_state(TASK_INTERRUPTIBLE);
	schedule();
	set_current_state(TASK_RUNNING);
	printk(" SWITCH:  tid[%u] woken up\n", tsk->pid );

	//get lock
	mutex_lock(&lock);
	//printk(" SWITCH: locked tid[%u]\n", tid);

	found = 1;
      }
    }
    temp = temp->next;
  }

  //

  mutex_unlock(&lock);
  //printk(" SWITCH: unlocked tid[%u]\n", tid);

  
  return 0;
}

/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */
int processor_container_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg)
{
    switch (cmd)
    {
    case PCONTAINER_IOCTL_CSWITCH:
        return processor_container_switch((void __user *)arg);
    case PCONTAINER_IOCTL_CREATE:
        return processor_container_create((void __user *)arg);
    case PCONTAINER_IOCTL_DELETE:
        return processor_container_delete((void __user *)arg);
    default:
        return -ENOTTY;
    }
}
