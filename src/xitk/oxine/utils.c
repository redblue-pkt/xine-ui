/*
 * Copyright (C) 2002-2003 Stefan Holst
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA.
 *
 * utilities
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>

#include "list.h"
#include "utils.h"

/*
 * multi purpose scheduler
 */

typedef struct {

  time_t start_time;
  xine_list_t *jobs;
  pthread_t scheduler_thread;
  pthread_mutex_t jobs_mutex;
  pthread_mutex_t job_execution_mutex;
  pthread_mutex_t wait_mutex;
  pthread_cond_t jobs_reorganize;
  int job_id;
  int keep_going;
  
} ox_scheduler_t;


/*
 * GLOBAL scheduler instance
 */
 
static ox_scheduler_t *ox_scheduler = NULL;


typedef struct {

  struct timespec ts; /* when to do */
  void (*cb)(void *data);
  void *data;
  int id;

} job_t;

static __attribute__((noreturn)) void *scheduler_thread(void *data) {

  job_t *job;
  int ret;
  void (*cb)(void *data);
  void *cb_data;

#ifdef LOG
  printf("utils: scheduler thread created\n");
#endif
 
  if (!ox_scheduler) {
    pthread_exit(NULL);
  }
  pthread_mutex_lock(&ox_scheduler->wait_mutex);

  while (ox_scheduler->keep_going) {
    
    pthread_mutex_lock(&ox_scheduler->jobs_mutex);
    job = xine_list_last_content(ox_scheduler->jobs);
    
    if (!job) { /* no jobs for me */
      pthread_mutex_unlock(&ox_scheduler->jobs_mutex);
#ifdef LOG
      printf("utils: no jobs in queue\n");
#endif
      pthread_cond_wait(&ox_scheduler->jobs_reorganize, &ox_scheduler->wait_mutex);
      continue;
    } else pthread_mutex_unlock(&ox_scheduler->jobs_mutex);

#ifdef LOG
    printf("utils: sleeping until next job\n");
#endif
    ret = pthread_cond_timedwait(&ox_scheduler->jobs_reorganize, 
	                         &ox_scheduler->wait_mutex, &job->ts);

    if (ret == ETIMEDOUT) {
      /* lets do a job */
      pthread_mutex_lock(&ox_scheduler->job_execution_mutex);
      pthread_mutex_lock(&ox_scheduler->jobs_mutex);
      job = xine_list_last_content(ox_scheduler->jobs);
      cb = NULL;
      cb_data = NULL;
      if (job) {
#ifdef LOG
        printf("utils: executing job %i\n", job->id);
#endif
        cb = job->cb;
        cb_data = job->data;
      }
      xine_list_delete_current(ox_scheduler->jobs);
      pthread_mutex_unlock(&ox_scheduler->jobs_mutex);

      if (cb) cb(cb_data);

      pthread_mutex_unlock(&ox_scheduler->job_execution_mutex);
      
    } else {
#ifdef LOG
      printf("utils: reorganizing queue\n");
#endif
    }
  }
  pthread_mutex_unlock(&ox_scheduler->wait_mutex);
  pthread_exit(NULL);
}

void start_scheduler(void) {

  struct timeval tv;
  struct timezone tz;

  ox_scheduler = malloc(sizeof(ox_scheduler_t));
  
  gettimeofday(&tv, &tz);
  ox_scheduler->start_time = tv.tv_sec;

  ox_scheduler->jobs = xine_list_new();
  pthread_mutex_init(&ox_scheduler->wait_mutex, NULL);
  pthread_mutex_init(&ox_scheduler->jobs_mutex, NULL);
  pthread_mutex_init(&ox_scheduler->job_execution_mutex, NULL);
  pthread_cond_init(&ox_scheduler->jobs_reorganize, NULL);
  ox_scheduler->job_id = 0;
  ox_scheduler->keep_going = 1;

  if (pthread_create(&ox_scheduler->scheduler_thread, NULL, scheduler_thread, ox_scheduler)) {
    printf("utils: error creating scheduler thread\n");
    abort();
  }
}

void stop_scheduler(void) {

  job_t *job;
  void *ret = NULL;
  
  if (!ox_scheduler) return;
  
  ox_scheduler->keep_going = 0;
  pthread_cond_signal(&ox_scheduler->jobs_reorganize);

  pthread_join(ox_scheduler->scheduler_thread, ret);
  
  job = xine_list_first_content(ox_scheduler->jobs);

  while(job) {
#ifdef LOG
    printf("utils: cancelling pending job %i\n", job->id);
#endif
    free(job);
    job = xine_list_next_content(ox_scheduler->jobs);
  }
  xine_list_free(ox_scheduler->jobs);
  pthread_mutex_destroy(&ox_scheduler->wait_mutex);
  pthread_mutex_destroy(&ox_scheduler->jobs_mutex);
  pthread_cond_destroy(&ox_scheduler->jobs_reorganize);

  free(ox_scheduler);
  ox_scheduler = NULL;
}

int schedule_job(int delay, void (*cb)(void *data), void *data) {
  
  struct timeval tv;
  struct timezone tz;
  job_t *job = malloc(sizeof(job_t));
  int msec;
  int priority;
  
  if (!ox_scheduler) return -1;

  gettimeofday(&tv, &tz);
  
  job->ts.tv_sec = (delay / 1000) + tv.tv_sec;
  msec = delay % 1000;
  if ((msec + tv.tv_usec/1000)>=1000) job->ts.tv_sec++;
  msec = (msec + tv.tv_usec/1000) % 1000;
  job->ts.tv_nsec = msec * 1000000;

  job->cb = cb;
  job->data = data;
  job->id = ++ox_scheduler->job_id;
  
  priority = (job->ts.tv_sec - ox_scheduler->start_time) * 1000 + msec;

  pthread_mutex_lock(&ox_scheduler->jobs_mutex);
  xine_list_append_priority_content(ox_scheduler->jobs, job, priority);
  pthread_mutex_unlock(&ox_scheduler->jobs_mutex);
  
  pthread_cond_signal(&ox_scheduler->jobs_reorganize);

  return ox_scheduler->job_id;
}

void cancel_job(int job_id) {

  job_t *job;
  
  if (!ox_scheduler) return;

  pthread_mutex_lock(&ox_scheduler->jobs_mutex);

  job = xine_list_first_content(ox_scheduler->jobs);

  while(job) {
    if (job->id == job_id) break;
    job = xine_list_next_content(ox_scheduler->jobs);
  }
  if (job) {
    xine_list_delete_current(ox_scheduler->jobs);
#ifdef LOG
    printf("utils: job %i cancelled\n", job->id);
#endif
    free(job);
  }
  pthread_mutex_unlock(&ox_scheduler->jobs_mutex);

  pthread_cond_signal(&ox_scheduler->jobs_reorganize);
}

void lock_job_mutex(void) {
  if (!ox_scheduler) return;
  pthread_mutex_lock(&ox_scheduler->job_execution_mutex);
}

void unlock_job_mutex(void) {
  if (!ox_scheduler) return;
  pthread_mutex_unlock(&ox_scheduler->job_execution_mutex);
}

/*
 * heap management
 */

/*
 * Heap objects are aligned on sizeof(int) boundaries
 */

#define ALIGNMENT (sizeof(int))
#define DOALIGN(num) (((num)+ALIGNMENT-1)&~(ALIGNMENT-1))

/*
 * tag datastructures
 */

typedef struct prefix_tag_s  prefix_tag_t;
typedef struct postfix_tag_s postfix_tag_t;

struct prefix_tag_s {
  prefix_tag_t *prev;         /* previous object in heap      */
  prefix_tag_t* next;         /* next object in heap          */
  postfix_tag_t* postfix;     /* ptr to postfix object        */
  char* filename;             /* filename ptr or NULL         */
  long line;                  /* line number or 0             */
  size_t size;                /* size of allocated block      */
  void* content;              /* _gen_malloc() ptr of object  */
  char* tag;                  /* description string or NULL   */
};

struct postfix_tag_s {
  prefix_tag_t* prefix;
};

/*
 * GLOBAL: Points to first object in linked list of heap objects
 */

static prefix_tag_t* heap_head=NULL;


static void AddToLinkedList      ( prefix_tag_t* );
static void RemoveFromLinkedList ( prefix_tag_t* );
static void RenderDesc           ( prefix_tag_t*, char* );


void *_gen_malloc(size_t wSize, const char* tag, char* lpFile, int nLine) {
  
  prefix_tag_t* prefix;
  
  wSize = DOALIGN(wSize);
  prefix=(prefix_tag_t*)malloc(sizeof(prefix_tag_t)+wSize+sizeof(postfix_tag_t));
  if (prefix) {
    AddToLinkedList( prefix );
    prefix->postfix = (postfix_tag_t*)((char*)(prefix+1)+wSize);
    prefix->postfix->prefix = prefix;
    prefix->filename = lpFile;
    prefix->line = nLine;
    prefix->content = prefix+1;
    prefix->size = wSize;
    prefix->tag = NULL;
    if (tag) prefix->tag = strdup(tag);
    memset( prefix->content, 0, wSize );
    }
  else {
    printf("utils: failed to alloc memory\n");
    abort();
  }
  
  return(prefix ? prefix+1 : NULL);
}


void *_gen_free(void* content) {
  
  if (ho_verify(content)) {
    
    prefix_tag_t* prefix=(prefix_tag_t*)content-1;
    size_t        wSize=(char*)(prefix->postfix+1)-(char*)prefix;
    
    RemoveFromLinkedList( prefix );
    free(prefix->tag);
    memset( prefix, 0, wSize );
    free(prefix);
  }

  return (NULL);
}


void *_gen_strdup(const char* lpS, char* lpFile, int nLine) {
  
  void* lpReturn=NULL;

  if (lpS) {
    size_t wSize = (size_t)(strlen(lpS)+1);
    
    lpReturn = _gen_malloc( wSize, "strdup'ed string", lpFile, nLine );
    if (lpReturn) {
      memcpy( lpReturn, lpS, wSize );
    }
  }
  return(lpReturn);

}


void *_gen_realloc(void* lpOld, size_t wSize, char* lpFile, int nLine) {

  void* lpNew=NULL;

  /*--- Try to realloc ---*/
  if (lpOld) {
    if (ho_verify(lpOld)) {
      prefix_tag_t* prefix=(prefix_tag_t*)lpOld-1;
      prefix_tag_t* lpNewPrefix;
      prefix_tag_t* lpPre;

      /*--- Try to reallocate block ---*/
      RemoveFromLinkedList( prefix );
      memset( prefix->postfix, 0, sizeof(postfix_tag_t) );
      wSize = DOALIGN(wSize);
      lpNewPrefix=(prefix_tag_t*)realloc(prefix,
          sizeof(prefix_tag_t)+wSize+sizeof(postfix_tag_t));

      /*--- Add new (or failed old) back in ---*/
      lpPre=(lpNewPrefix?lpNewPrefix:prefix);
      AddToLinkedList( lpPre );
      lpPre->postfix = (postfix_tag_t*)((char*)(lpPre+1)+wSize);
      lpPre->postfix->prefix = lpPre;
      lpPre->content = lpPre+1;
      lpPre->size = wSize;

      /*--- Finish ---*/
      lpNew = (lpNewPrefix ? &lpNewPrefix[1] : NULL);
      if (!lpNew) {
	printf("utils: failed to alloc memory\n");
	abort();
      }
    }
  }

  /*--- Else try new allocation ---*/
  else {
    lpNew = _gen_malloc( wSize, NULL, lpFile, nLine );
    }

  /*--- Return address to object ---*/
  return(lpNew);

}


void heapstat(void) {
 
  unsigned long total = 0;
  unsigned long chunks = 0;
  if (heap_head) {
    prefix_tag_t* lpCur=heap_head;
    
    while (ho_verify(&lpCur[1])) {
      char buffer[100];
      
      RenderDesc( lpCur, buffer );
      /*--- print out buffer ---*/
      printf( "heapstat: %s\n", buffer );
      total+=lpCur->size;
      chunks++;
      lpCur = lpCur->next;
      if (lpCur==heap_head) {
        break;
      }
    }
    if (total) 
      printf("heapstat: memory usage: %li words in %li chunks\n", total, chunks);
  }
}


static void AddToLinkedList(prefix_tag_t* lpAdd) {
    /*--- Add before current head of list ---*/
    if (heap_head) {
        lpAdd->prev = heap_head->prev;
        (lpAdd->prev)->next = lpAdd;
        lpAdd->next = heap_head;
        (lpAdd->next)->prev = lpAdd;
        }

    /*--- Else first node ---*/
    else {
        lpAdd->prev = lpAdd;
        lpAdd->next = lpAdd;
        }

    /*--- Make new item head of list ---*/
    heap_head = lpAdd;
}

static void RemoveFromLinkedList(prefix_tag_t* lpRemove) {

    /*--- Remove from doubly linked list ---*/
    (lpRemove->prev)->next = lpRemove->next;
    (lpRemove->next)->prev = lpRemove->prev;

    /*--- Possibly correct head pointer ---*/
    if (lpRemove==heap_head) {
        heap_head = ((lpRemove->next==lpRemove) ? NULL : 
            lpRemove->next);
        }
}

static int ho_is_ok(void* content)
{
    return ((content) && (!((long)content&(ALIGNMENT-1))));
}


int ho_verify(void *content) {
  int bOk=0;

  if (content) {
    if (ho_is_ok(content)) {
      prefix_tag_t* prefix=(prefix_tag_t*)content-1;
      if(prefix->content==content) {
        if(prefix->postfix->prefix==prefix) {
          bOk = 1;
	}
      }
    }
  }
  return (bOk);
}


static void RenderDesc(prefix_tag_t* prefix, char* lpBuffer) {
  if (prefix->content==&prefix[1]) {
    sprintf( lpBuffer, "%zi words @ 0x%08lx:", prefix->size, (unsigned long)prefix->content );
    if (prefix->filename) {
      sprintf( lpBuffer+strlen(lpBuffer), "%s:%ld ",
	  prefix->filename, prefix->line );
    }
    if (prefix->tag)
      strcat(lpBuffer, prefix->tag);
  }
  else {
    strcpy( lpBuffer, "(bad pointer given)" );
  }
}
