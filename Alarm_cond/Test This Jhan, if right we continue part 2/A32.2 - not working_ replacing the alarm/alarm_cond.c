/*Completed up to and including part: A3.2.1*/

/*
 * alarm_cond.c
 *
 * This is an enhancement to the alarm_mutex.c program, which
 * used only a mutex to synchronize access to the shared alarm
 * list. This version adds a condition variable. The alarm
 * thread waits on this condition variable, with a timeout that
 * corresponds to the earliest timer request. If the main thread
 * enters an earlier timeout, it signals the condition variable
 * so that the alarm thread will wake up and process the earlier
 * timeout first, requeueing the later request.
 */
#include <pthread.h>
#include <time.h>
#include "errors.h"

/*
 * The "alarm" structure now contains the time_t (time since the
 * Epoch, in seconds) for each alarm, so that they can be
 * sorted. Storing the requested number of seconds would not be
 * enough, since the "alarm thread" cannot tell how long it has
 * been on the list.
 */
typedef struct alarm_tag {
    struct alarm_tag    *link;
    int                 seconds;
    time_t              time;   /* seconds from EPOCH */
    int                 Message_Number;
    char                message[64];
} alarm_t;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_cond = PTHREAD_COND_INITIALIZER;
alarm_t *alarm_list = NULL;
time_t current_alarm = 0;

/*Will be used to find alarms in the alarm_list*/
alarm_t *temp, *prev;

/*
 * Insert alarm entry on list, in order.
 */
void alarm_insert (alarm_t *alarm)
{
    int status;
    alarm_t **last, *next;

    /*
     * LOCKING PROTOCOL:
     * 
     * This routine requires that the caller have locked the
     * alarm_mutex!
     */

    last = &alarm_list;
    next = *last;

    /*Everytime the insert function is used, when a user inputs a new alarm,
      temp will point the alarm list. If it doesnt, it will just point to the
      old reference that alarm_list used to point to, but doesnt point there anymore.

      Basically this assignment assures that temp points to an updated alarm_list.*/
    temp = alarm_list;
    
    while (next != NULL) {
        if (next->time >= alarm->time) {
            alarm->link = next;
            *last = alarm;
            break;
        }
        last = &next->link;
        next = next->link;
    }

    /*
     * If we reached the end of the list, insert the new alarm
     * there.  ("next" is NULL, and "last" points to the link
     * field of the last item, or to the list header.)
     */
    if (next == NULL) {
        *last = alarm;
        alarm->link = NULL;
    }
#ifdef DEBUG
    printf ("[list: ");
    for (next = alarm_list; next != NULL; next = next->link)
        printf ("%d(%d)[\"%s\"] ", next->time,
            next->time - time (NULL), next->message);
    printf ("]\n");
#endif
    /*
     * Wake the alarm thread if it is not busy (that is, if
     * current_alarm is 0, signifying that it's waiting for
     * work), or if the new alarm comes before the one on
     * which the alarm thread is waiting.
     */
    if (current_alarm == 0 || alarm->time < current_alarm) {
        current_alarm = alarm->time;
        status = pthread_cond_signal (&alarm_cond);
        if (status != 0)
            err_abort (status, "Signal cond");
    }
}

/*
 * The alarm thread's start routine.
 */
void *alarm_thread (void *arg)
{
    alarm_t *alarm;
    struct timespec cond_time;
    time_t now;
    int status, expired;

    /*
     * Loop forever, processing commands. The alarm thread will
     * be disintegrated when the process exits. Lock the mutex
     * at the start -- it will be unlocked during condition
     * waits, so the main thread can insert alarms.
     */
    status = pthread_mutex_lock (&alarm_mutex);
    if (status != 0)
        err_abort (status, "Lock mutex");
    while (1) {
        /*
         * If the alarm list is empty, wait until an alarm is
         * added. Setting current_alarm to 0 informs the insert
         * routine that the thread is not busy.
         */
        current_alarm = 0;
        while (alarm_list == NULL) {
            status = pthread_cond_wait (&alarm_cond, &alarm_mutex);
            if (status != 0)
                err_abort (status, "Wait on cond");
            }
        alarm = alarm_list;
        alarm_list = alarm->link;

        now = time (NULL);
        expired = 0;
        if (alarm->time > now) {
#ifdef DEBUG
            printf ("[waiting: %d(%d)\"%s\"]\n", alarm->time,
                alarm->time - time (NULL), alarm->message);
#endif
            cond_time.tv_sec = alarm->time;
            cond_time.tv_nsec = 0;
            current_alarm = alarm->time;
            while (current_alarm == alarm->time) {
                status = pthread_cond_timedwait (
                    &alarm_cond, &alarm_mutex, &cond_time);
                if (status == ETIMEDOUT) {
                    expired = 1;
                    break;
                }
                if (status != 0)
                    err_abort (status, "Cond timedwait");
            }
            if (!expired)
                alarm_insert (alarm);
        } else
            expired = 1;
        if (expired) {
            printf ("%d Message(%d) %s\n", alarm->seconds, alarm->Message_Number, alarm->message);
            free (alarm);
        }
    }
}

int main (int argc, char *argv[])
{
    int status;
    char line[128];
    alarm_t *alarm;
    pthread_t thread;

    /*Modified Jhan's original Struct.
      Took out one of the members of the Struct.
      Put it in Main Method.*/
    int Find_Message = -1;

    //0 - no, 1 - yes
    int sameMesNum = 0;
    temp = alarm_list;

    status = pthread_create (
        &thread, NULL, alarm_thread, NULL);
    if (status != 0)
        err_abort (status, "Create alarm thread");
    while (1) {
        printf ("Alarm> ");
        if (fgets (line, sizeof (line), stdin) == NULL) exit (0);
        if (strlen (line) <= 1) continue;
        alarm = (alarm_t*)malloc (sizeof (alarm_t));
        if (alarm == NULL)
            errno_abort ("Allocate alarm");

        /*
         * Parse input line into seconds (%d) and a message
         * (%64[^\n]), consisting of up to 64 characters
         * separated from the seconds by whitespace.
         */
        if (sscanf (line, "%d Message(%d) %64[^\n]", 
            &alarm->seconds, &alarm->Message_Number, alarm->message) < 3) {
            //Read in the Cancel Command if the message isn't a set command
            if(sscanf (line, "Cancel: Message(%d)", &Find_Message) < 1)
            {
                //Print out "Bad Command" if wrong input format.
                fprintf (stderr, "Bad command\n");
                //Free the alarm thread.
                free (alarm);
            }
            else{
                printf("Looking for alarm to cancel\n");
                /*Find the alarm with the Message_Number == to Find_Message*/
            }
        }else {
            printf("Making the alarm\n");
            status = pthread_mutex_lock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Lock mutex");
            alarm->time = time (NULL) + alarm->seconds;

            /*
             * The main function prints out this message when a user enters an alarm. */
            printf("Alarm Request Received at <%d>:<%d %s>\n", (alarm->time - alarm->seconds), alarm->seconds, alarm->message);
            /*
             * Insert the new alarm into the list of alarms,
             * sorted by expiration time.
             */

             prev = temp;
             /*Will find if there is another alarm with the same Message Number*/
             while(temp != NULL)
             {
                /*If new alarm's message_number == another message_number of an alarm already in the list,
                  then break the while loop. If temp points to NULL, then alarm->message_number is unique*/
                if(alarm->Message_Number == temp->Message_Number)
                {
                    sameMesNum = 1;
                    break;
                }
                prev = temp;
                temp = temp->link;
             }

             /*If no alarm exists with same Message_Number as the new alarm to be inserted*/
             if(sameMesNum == 0)
                alarm_insert (alarm);
             else
             {
                printf("Alarm with Message Number(%d) EXISTS! Replacing that alarm.\n", alarm->Message_Number);



                /************************* A3.2.2 *************************/
                if(temp == prev) //first alarm in alarm_list must be replaced by new alarm
                {
                    if(temp->link == NULL)//Only one alarm in alarm_list
                        alarm_list = alarm;
                    else//More than one alarm in alarm_list
                    {
                        alarm->link = alarm_list->link;
                        alarm_list = alarm;
                    }
                }
                else//alarm to be replaced in alarm_list is not the first alarm
                {
                    if(temp->link == NULL)//alarm is the last alarm in the alarm_list
                    {
                        prev->link = alarm;
                    }
                    else//alarm in the middle somewhere in alarm_list
                    {
                        prev->link = alarm;
                        alarm->link = temp->link;
                    }
                }
                /************************* A3.2.2 *************************/



             }

            //If no alarm exists with same Message_Number as the new alarm to be inserted
            //alarm_insert (alarm);         <- on line 237

            status = pthread_mutex_unlock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Unlock mutex");
        }
        Find_Message = -1;
        sameMesNum = 0;
    }
}